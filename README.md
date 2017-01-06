ACPC Match Log
==================

A small C++14 library for parsing log files in the [Annual Computer Poker
Competition](http://www.computerpokercompetition.org/)'s `dealer`
[format](http://www.computerpokercompetition.org/downloads/documents/protocols/protocol.pdf).

See `test` for usage examples.

This library consists mostly of headers for easy inclusion in other projects.


Installation and Dependencies
-----------------------------

This library depends on three external libraries:

1. [*project_acpc_server*](http://www.computerpokercompetition.org/downloads/code/competition_server/project_acpc_server_v1.0.41.tar.bz2),
2. [*cpp_utilities*](https://github.com/dmorrill10/cpp_utilities), and
3. [*Catch*](https://github.com/philsquared/Catch) for testing.

`Brickfile` contains version and download specifications for these
dependencies. One can either download each manually, or use
[*BrickAndMortar*](https://github.com/dmorrill10/brick_and_mortar), a
[*Ruby*](https://www.ruby-lang.org/en/) application that automatically
manages `Brickfile` dependencies.

Running `make` will generate the `dealer` module's object file, as well as
object files for *project_acpc_server*'s modules, in `obj`.

Running `make test` will run this library's tests.


Modules
-------

The `acpc` module consists of general helper functions, `acpc_match_log` is the
log parsing interface, and `encapsulated_match_state` is the
match state representation produced by parsing functions.

The `dealer` module is the only one that must be compiled before use. It is
mostly a copy of the dealer code from *project_acpc_server*, except that it
exposes an application programming interface to run matches.


Contributing
------------

See the [issue tracker](https://github.com/dmorrill10/acpc_match_log/issues) for currently known issues, or to log new ones. Feel free to help make this library better
by contributing your own changes by:

1. Forking this library
2. Creating your feature branch (`git checkout -b my-new-feature`)
3. Committing your changes (`git commit -am 'Added some feature'`)
4. Including tests for your feature
5. Pushing to the branch (`git push origin my-new-feature`)
6. Creating new Pull Request

Please include tests for any changes, otherwise I may not be able to find time
to review your pull request.

License
-------
MIT license (see `LICENCE`).
