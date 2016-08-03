.. vim: ft=rst sw=2 sts=2 et tw=72
.. default-role:: literal

########################################################################
                                PROGLOG
########################################################################
========================================================================
                              Logging Tool
========================================================================

:Author: Roman Neuhauser
:Contact: neuhauser@sigpipe.cz
:Copyright: This document is in the public domain.

.. this file is marked up using reStructuredText
   lines beginning with ".." are reST directives
   "foo_" or "`foo bar`_" is a link, defined at ".. _foo" or ".. _foo bar"
   "::" introduces a literal block (usually some form of code)
   "`foo`" is some kind of identifier
   suspicious backslashes in the text ("`std::string`\s") are required for
   reST to recognize the preceding character as syntax

.. contents::
    :depth: 1

Synopsis
========

::

  proglog [--log=<LOGFILE>] <CMD> [<ARG>...]

Description
===========

`proglog` runs `CMD` with any arguments given, copying all inputs
and outputs into `LOGFILE`, each line preceded by a timestamp of
the moment it was received.

See `tai64n(8)`_ for details of the timestamp format.

.. _tai64n(8): http://cr.yp.to/daemontools/tai64n.html
