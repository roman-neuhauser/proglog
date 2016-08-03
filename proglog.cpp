// vim: sw=2 sts=2 et fdm=marker cms=\ //\ %s

#define _POSIX_C_SOURCE 201607

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <err.h>
#include <fcntl.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>

#define fail(f) \
  err(EXIT_FAILURE, #f)

namespace // {{{
{

typedef std::pair<int, int> Pipe;
typedef std::vector<Pipe> Pipes;

inline
void
setup_pipes(Pipes &pipes) // {{{
{
  for (auto &p: pipes) {
    int rp[2];
    if (pipe(rp) < 0)
        fail(pipe);
    p = std::make_pair(rp[0], rp[1]);
  }
} // }}}

namespace subject // {{{
{

struct Subject // {{{
{
  Subject(char *const argv[], Pipes const& pipes)
  {
    if (close(pipes[0].second) < 0) fail(close);
    if (close(pipes[1].first)  < 0) fail(close);
    if (close(pipes[2].first)  < 0) fail(close);

    if (dup2(pipes[0].first,  0) < 0) fail(dup2);
    if (dup2(pipes[1].second, 1) < 0) fail(dup2);
    if (dup2(pipes[2].second, 2) < 0) fail(dup2);

    execvp(*argv, argv);
    err(EXIT_FAILURE, "%s", *argv);
  }
}; // }}}

} // }}}

namespace watcher // {{{
{

typedef std::vector<int> Fds;
typedef std::map<int, Fds> Tees;

template <typename Map>
inline
auto
keys(Map &map) // {{{
{
  return map | boost::adaptors::map_keys;
} // }}}

#define TIMESTAMP 26

static
void
to_tai64nlabel(char buf[TIMESTAMP], struct timespec *tp) // {{{
{
  static char hex[17] = "0123456789abcdef";
  static char pack[12];
	uint64_t x;
  x = 4611686018427387914ULL + tp->tv_sec;
  for (int i = 7; i > -1; --i) {
    pack[i] = x & 255; x >>= 8;
  }
  x = tp->tv_nsec;
  for (int i = 11; i > 7; --i) {
    pack[i] = x & 255; x >>= 8;
  }
  buf[0] = '@';
  for (int i = 0;i < 12;++i) {
    buf[i * 2 + 1] = hex[(pack[i] >> 4) & 15];
    buf[i * 2 + 2] = hex[(pack[i] & 15)];
  }
  buf[TIMESTAMP-1] = ' ';
} // }}}

template <typename size_type>
inline
void
sink(Fds& fds, char* buf, size_type len) // {{{
{
  static struct timespec now;
  static char label[TIMESTAMP];
  if (clock_gettime(CLOCK_REALTIME, &now))
    err(EXIT_FAILURE, "clock_gettime");
  to_tai64nlabel(label, &now);

  auto nl = len;
  do {
    auto pos = strchr(buf, '\n');
    if (pos) nl = pos - buf;
    for (auto const &fd: fds) {
      if (write(fd, label, static_cast<size_t>(TIMESTAMP)) < 0)
        fail(write);
      if (write(fd, buf, static_cast<size_t>(nl + 1)) < 0)
        fail(write);
      fsync(fd);
    }
    buf = pos + 1;
    len -= nl + 1;
  } while (len);
} // }}}

inline
auto
done(Tees& tees, int fd) // {{{
{
  auto sinks = tees[fd];
  close(fd);
  for (size_t i = 1; i < sinks.size(); ++i)
    close(sinks[i]);
  tees.erase(tees.find(fd));
} // }}}

inline
auto
init(Tees& tees, Fds& sources) // {{{
{
  sources.clear();
  boost::copy(keys(tees), std::back_inserter(sources));
  return *std::max_element(sources.begin(), sources.end());
} // }}}

inline
auto
logfile(std::string const& fpath) // {{{
{
  return open(
    fpath.c_str()
  , O_CREAT|O_APPEND|O_WRONLY
  , S_IRUSR|S_IWUSR
  );
} // }}}

struct Watcher // {{{
{
  Watcher(std::string const& fpath, pid_t child, int argc, char* argv[], Pipes const& pipes) // {{{
  : dead(false)
  , logfd(logfile(fpath))
  , pipes(pipes)
  {
    log_cmd(argc, argv);

    Tees tees;
    tees.emplace(std::make_pair(0, Fds{logfd, pipes[0].second}));
    tees.emplace(std::make_pair(pipes[1].first, Fds{logfd, 1}));
    tees.emplace(std::make_pair(pipes[2].first, Fds{logfd, 2}));

    watch(child, tees);
  } // }}}
  ~Watcher() // {{{
  {
    if (0 > close(logfd))           fail(close);
    if (0 > close(pipes[0].second)) fail(close);
    if (0 > close(pipes[1].first))  fail(close);
    if (0 > close(pipes[2].first))  fail(close);
  } // }}}
private:
  bool dead;
  int logfd;
  Pipes const& pipes;
  static const ssize_t bufsz = 4096;
  char buf[bufsz];


  void
  log_cmd(int argc, char* argv[]) // {{{
  {
    static struct timespec now;
    static char label[TIMESTAMP];
    if (clock_gettime(CLOCK_REALTIME, &now))
      err(EXIT_FAILURE, "clock_gettime");
    to_tai64nlabel(label, &now);

    write(logfd, label, TIMESTAMP);
    write(logfd, "$", 1);
    for (auto i = 0; i < argc; ++i) {
      auto arg = argv[i];
      write(logfd, " ", 1);
      write(logfd, arg, strlen(arg));
    }
    write(logfd, "\n", 1);
  } // }}}

  void
  watch(pid_t child, Tees &tees) // {{{
  {
    Fds sources;

    int status = 0;

    fd_set rset;

    for (auto const &fd: keys(tees))
      fcntl(fd, F_SETFL, O_NONBLOCK|fcntl(fd, F_GETFL, 0));

    while (1) {
      auto maxfd = init(tees, sources);

      FD_ZERO(&rset);

      for (auto const &fd: sources) FD_SET(fd, &rset);

      if (pselect(1 + maxfd, &rset, 0, 0, 0, 0) < 0)
        fail(pselect);

      for(auto const &fd: sources) {
        if (!FD_ISSET(fd, &rset)) {
          continue;
        }
        if (!consume(fd, tees)) {
          continue;
        }
      }
      if (!dead && died(child, &status)) continue;
      if (dead) break;
    }

    if (WIFEXITED(status)) {
      exit(WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status))
      err(EXIT_FAILURE, "terminated by signal %d\n", WTERMSIG(status));
  } // }}}

  bool
  consume(int fd, Tees& tees) // {{{
  {
    while (true) {
      auto len = read(fd, buf, bufsz);
      if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return false;
        }
        fail(read);
      }
      if (len == 0) {
        done(tees, fd);
        return false;
      }
      sink(tees[fd], buf, len);
    }
    return true;
  } // }}}

  int
  died(pid_t child, int *status) // {{{
  {
    auto rv = waitpid(child, status, WNOHANG);
    if (rv < 0) fail(waitpid);
    dead = !!rv;
    return rv;
  } // }}}
}; // }}}

} // }}}

} // }}}

int
main(int argc, char* argv[]) // {{{
{
  if (argc < 2)
    errx(EXIT_FAILURE, "usage: %s [--log=<PATH>] <PROG> [<ARG>...]", argv[0]);

  std::string log = argv[1];
  if (log == "--log") {
    log = argv[2];
    argc -= 3;
    argv += 3;
  } else if (boost::starts_with(log, "--log=")) {
    log = log.substr(6);
    argc -= 2;
    argv += 2;
  } else {
    log = "transcript";
    argc -= 1;
    argv += 1;
  }

  Pipes pipes(3);
  setup_pipes(pipes);

  auto child = fork();

  if (child < 0)
    fail(fork);

  {
    if (child == 0)
      subject::Subject s{argv, pipes};
    else
      watcher::Watcher w{log, child, argc, argv, pipes};
  }

  abort(); // not reached
} // }}}
