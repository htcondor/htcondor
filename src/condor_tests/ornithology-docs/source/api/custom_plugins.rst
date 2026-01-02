File Transfer Plugins
=====================

Ornithology comes with the following built in file transfer plugins.
These plugins are part of the default configuration for all HTCondors
started by Ornithology unless overridden with custom configuration
making them readily available for jobs to use for file transfer.
However, the plugins can also be referred to by their :doc:`predefined`.

NULL Plugin
-----------

The NULL plugin copies all data from a file located on disk to
``/dev/null`` (or equivalent). This allows transferring of a file
without actually copying the file elsewhere on disk. For input
transfers the corresponding file will be created but left empty.

The NULL plugin accepts transfers using the ``null://`` URL schema.

.. warning::

    Transferred files need to exist on disk at transfer time.

Debug Plugin
------------

The Debug plugin that does not transfer any files but rather decodes
information encoded in the URL to decide how to act or what result
ClassAds to output.

The Debug plugin accepts transfers using the following URL schemas:

1. ``debug://``
2. ``decode://``
3. ``encoded://``

URL Encoding
~~~~~~~~~~~~

The URL has two levels of encoding. The first level of encoding is
the main command immediately proceeding the URL schema followed by
additional details which are forward slash (``/``) separated e.g.
``debug://command/(details)``. The details may be optional depending
on the command. The following are the available main commands:

*ECHO*
    - **Format**: ``schema://echo/<stream>/<message>``
    - **Example**: ``debug://echo/stdout/hello%20world``
    - **Description**: Inform plugin to log a specified message to
      a specific logging stream (standard output or error). All
      characters after the dividing backwards slash are considered
      part of the desired message and will be used literally. Only
      non-escaped ``%20``\'s will be replaced with spaces. The desired
      logging stream, which is required, can be specified with the
      following key words:

      *Standard Output*
          1. **STDOUT**
          2. **OUTPUT**
          3. **OUT**

      *Standard Error*
          1. **STDERR**
          2. **ERROR**
          3. **ERR**

*EXIT*
    - **Format**: ``schema://exit/<value>``
    - **Example**: ``debug://exit/1``
    - **Description**: Informs the plugin to exit immediately with an
      intended exit code specified in the extra details (immediately
      following the command). The plugin will always exit when this
      command is specified but may not exit with the specified number
      if:

      1. Any previously parsed URL had an invalid encoding (This is a
         multi-file plugin, of course).
      2. The provided value was negative, in which case it exits with
         the absolute value.
      3. The provided value was out of the range of ``0`` - ``123``.
         Will choose the closest exit code in that range (likely ``123``).

*SLEEP*
    - **Format**: ``schema://sleep/<value>``
    - **Example**: ``debug://sleep/20``
    - **Description**: Informs the plugin to sleep for the specified time
      immediately following the command. Once the sleep has finished, the
      plugin will write a successful result ClassAd to ``stdout``. If only
      a sleep URL is provided then the plugin will exit successfully.

*SIGNAL*
    - **Format**: ``schema://signal/<value>``
    - **Example**: ``debug://signal/SIGKILL``
    - **Description**: Inform the plugin to immediately send the signal
      specified immediately after the command to itself. The available
      signal values are denoted by `python signal module <https://docs.python.org/3/library/signal.html#signal.SIG_DFL>`_

*VERBOSITY*
    - **Format**: ``schema://verbosity/<level>``
    - **Example**: ``debug://verbosity/debugging``
    - **Description**: Set the debug plugins current logging level. See
      :ref:`debug_plugin_logging` for more information.

*SUCCESS* and *ERROR*
    - **Format**: ``schema://command/(details)`` [Details are optional]
    - **Examples**: ``debug://success``, ``debug://error/``, ``debug://error/general[foo=True;]``
    - **Description**: Denotes whether a transferred file was successful or
      failed due to an error. If an **error** command is in the transfer URL
      list then the plugin will exit with ``1`` unless an *EXIT* encoded URL
      follows in transfer order or an internal failure occurred.

      Both of these commands will write a file transfer result ClassAd to
      the output file. This ClassAd will be constructed of default values
      and whatever further ClassAd details are encoded in the additional
      details (see detail encoding below).

**Special Details Encoding**
    Details about how to update the returned ClassAd for a transfer URL via
    the plugin can be encoded in portions of the URL (``schema://command/portion/portion/portion``)
    using specified keywords ('sub commands'). Any portion of the URL not
    beginning with a designated keyword is ignored. Parsing of the encoding
    happens left to right of the URL; meaning later keywords details take
    precedence. The following keywords can be specified in URL portions:

    *GENERAL*
        - **Format**: ``general([details])`` (``[details]`` is optional).
        - **Example**: ``debug://error/general`` (default update ClassAd)
        - **Description**: Update/insert ClassAd attributes associated in
          the main body of the result ad. (see detail encoding below)

    *PARAMETER*
        - **Format**: ``parameter([details])`` (``[details]`` is optional).
        - **Example**: ``debug://error/paramter`` (default update ClassAd)
        - **Description**: Create a ``Parameter`` error type ClassAd to add
          to the ``TransferErrorData`` attribute. The added ClassAd will be
          a default ad updated via any encoded information. (see detail
          encoding below)

    *RESOLUTION*
        - **Format**: ``resolution([details])`` (``[details]`` is optional).
        - **Example**: ``debug://error/resolution`` (default update ClassAd)
        - **Description**: Create a ``Resolution`` error type ClassAd to add
          to the ``TransferErrorData`` attribute. The added ClassAd will be
          a default ad updated via any encoded information. (see detail
          encoding below)

    *CONTACT*
        - **Format**: ``contact([details])`` (``[details]`` is optional).
        - **Example**: ``debug://error/contact`` (default update ClassAd)
        - **Description**: Create a ``Contact`` error type ClassAd to add
          to the ``TransferErrorData`` attribute. The added ClassAd will be
          a default ad updated via any encoded information. (see detail
          encoding below)

    *AUTHORIZATION*
        - **Format**: ``authorization([details])`` (``[details]`` is optional).
        - **Example**: ``debug://error/authorization`` (default update ClassAd)
        - **Description**: Create a ``Authorization`` error type ClassAd to add
          to the ``TransferErrorData`` attribute. The added ClassAd will be
          a default ad updated via any encoded information. (see detail encoding
          below)

    *SPECIFICATION*
        - **Format**: ``specification([details])`` (``[details]`` is optional).
        - **Example**: ``debug://error/specification`` (default update ClassAd)
        - **Description**: Create a ``Specification`` error type ClassAd to add
          to the ``TransferErrorData`` attribute. The added ClassAd will be
          a default ad updated via any encoded information. (see detail encoding
          below)

    *TRANSFER*
        - **Format**: ``transfer([details])`` (``[details]`` is optional).
        - **Example**: ``debug://error/transfer`` (default update ClassAd)
        - **Description**: Create a ``Transfer`` error type ClassAd to add
          to the ``TransferErrorData`` attribute. The added ClassAd will be
          a default ad updated via any encoded information. (see detail
          encoding below)

    *DELETE*
        - **Format**: ``delete[attr(,attr...)]``
        - **Example**: ``debug://success/general[foo=1;bar=2;]/delete[bar]``
        - **Description**: Delete attribute(s) from the main body of the
          ClassAd that will be used to update the result ClassAd.

        .. note::

            This does not delete any of the default result ad attributes or
            attributes in nested ClassAd's such as the error ads.

    **Details Encoding**
        The majority of the sub commands listed above support an optional
        details field. The entire field is set off by square brackets (``[]``)
        and is optionally prefixed by a  character denoting the details
        source type (``~`` or ``#``). The URL contents of the field depend
        on he specified source type:

        1. Inline: The update ClassAd is encoded directly into the URL.
            - **Format**: ``keyword[ClassAd]``
            - **Example**: ``debug://error/general[foo=2;bar=True;baz="My%20String";Nested=[Pi=3.14;];]``

            .. note::
                Nested ClassAds are acceptable in this encoding (i.e. ``DeveloperData``).

        2. File: The update ClassAd can be parsed from an external file.
            - **Format**: ``keyword~[path/to/file]``
            - **Example**: ``debug://error/contact~[contact.err.ad]``

        3. Executable: The update ClassAd can be parsed from the ``stdout``
           of a specified executable.

              - **Format**: ``keyword#[path/to/executable]``
              - **Example**: ``debug://success/transfer#[./generate_ad.sh]``

        .. note::

            Only a single ClassAd is parsed from both the file and executable
            specifications.

        .. note::

            Any trailing characters after the closing bracket for a portion
            are ignored i.e. ``general[foo=True;]this-is-ignored/contact``.

        *Encoding Spaces*
            URL's can't contain whitespace. To combat this for encoding details
            in the plugin URL you can use the URL standard ``%20``. All ``%20``\'s
            in the URL are replace with spaces.

            Inline details will also replace ``+`` with a space by default
            unless the ``+`` is escaped i.e. ``\+``.

            Executables will also replace ``::`` with a space by default. This
            combined with ``%20`` can be used to specify arguments such as
            ``debug://error/general#[./script.sh::-count::20]``

.. note::

    All main commands and sub commands are case insensitive.

.. note::

    There is only one main command per file transfer URL.

.. _debug_plugin_env_vars:

Environment Variables
~~~~~~~~~~~~~~~~~~~~~

The debug plugin checks for the following environment variables to control
various aspects of execution:

**DEBUG_PLUGIN_PROTOCOL_VERSION**
    An integer value representing the HTCondor plugin protocol version
    to speak. Set to ``4`` by default.

**DEBUG_PLUGIN_TIMESTAMP_FORMAT**
    A string value specifying the logging timestamp format following
    `python's DateTime format codes <https://docs.python.org/3/library/datetime.html#strftime-and-strptime-format-codes>`_.
    If provided format is invalid then the default value is used.
    This is set to ``%m/%d/%Y %H:%M:%S`` by default e.g. ``12/25/2025 07:33:42``.

**DEBUG_PLUGIN_VERBOSITY**
    A string or integer value representing the desired logging level (see
    :ref:`debug_plugin_logging`). Set to ``ALWAYS`` by default.

.. _debug_plugin_logging:

Logging
~~~~~~~

By default, the debug plugin will always print error messages to standard
error. However, a verbosity level can be specified in a variety of ways to
enable logging to standard output:

1. Via the ``-v/-verbose`` command line option.
2. Via an environment variable (see :ref:`debug_plugin_env_vars`)
3. Via an encoded URL using the **VERBOSITY** command.

It is possible to disable all logging by setting the logging level to a
negative level.

.. warning::

    Disabling logging will also prevent any **ECHO** command messages to
    standard output to not be printed.

The logging level can be set by specifying either the literal level name
or the equivalent integer value:

.. list-table::
    :header-rows: 1
    :widths: 30 15 100

    * - Level
      - Value
      - Description
    * - ALWAYS
      - 0
      - Logging to always print (**ECHO** command)
    * - TEST
      - 1
      - Logging intended for testing purposes
    * - DEBUGGING
      - 2
      - Enable general debugging messages
    * - VERBOSE
      - 3
      - Enable more detailed debugging messages
    * - CHATTY
      - 4
      - Enable all debugging messages

.. note::

    The logging level is print all messages below and at the specified levels.

Special Return Codes
~~~~~~~~~~~~~~~~~~~~

The Debug URL has two special return codes to help denote an actual
failure with the plugin opposed from the plugin being specified.
These codes are as follows:

1. ``124`` - Invalid URL encoding.
2. ``125`` - Some failure has occurred with the plugin (i.e. invalid arguments, failed to open input/output file, etc.)

