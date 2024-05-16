lsimport os
import sys
import time
import logging
import argparse
from pathlib import Path

from htcondor_cli import NOUNS, GLOBAL_OPTIONS


# Override ArgumentParser to not exit on error
# and instead try to re-raise any errors,
# or just do the default behavior if there's nothing to re-raise
class ArgumentParserNoExit(argparse.ArgumentParser):
    def error(self, message):
        try:
            raise
        except RuntimeError:
            # This can't be the right way to do this.
            if 'the following arguments are required' in message:
                if 'queue@system' in message:
                    noun = NOUNS['annex']
                    verb = getattr(noun, 'systems')
                    verb(logger=None)
            super().error(message)


def parse_args():
    base_parser = ArgumentParserNoExit(
        description="A tool for managing HTCondor jobs and resources.",
    )

    # Add global options
    for option_name in GLOBAL_OPTIONS:
        kwargs = GLOBAL_OPTIONS[option_name].copy()
        args = kwargs.pop("args")
        base_parser.add_argument(*args, **kwargs)

    base_subparser = base_parser.add_subparsers(description="",
        help="Types of HTCondor objects that can be managed",
        dest="noun",
    )

    # Add nouns to parser
    noun_parsers = {}
    for noun, noun_cls in NOUNS.items():
        noun_parser = base_subparser.add_parser(noun, description=noun_cls.__doc__)
        noun_parsers[noun] = noun_parser
        noun_subparser = noun_parser.add_subparsers(description="",
            help=f"Types of actions you can take on {noun}{'s' if not noun.endswith('x') else 'es'}",
            dest="verb",
        )

        # Add verbs to parser
        for verb_cls in noun_cls.verbs():
            verb = verb_cls.__name__
            verb_parser = noun_subparser.add_parser(verb, description=verb_cls.__doc__)
            for option_name in verb_cls.options:
                kwargs = verb_cls.options[option_name].copy()
                args = kwargs.pop("args")
                verb_parser.add_argument(*args, **kwargs)

    # Parse args and get the dict representation
    try:
        parsed_args = base_parser.parse_args()
        parsed_vars = vars(parsed_args)
    except argparse.ArgumentError as e:
        if "argument noun" in str(e):
            error_msg = f"{base_parser.format_help()}\n{str(e).replace('argument noun: ', '')}"
        elif "argument verb" in str(e):
            error_msg = f"{noun_parser.format_help()}\n{str(e).replace('argument verb: ', '')}"
        else:
            error_msg = f"{base_parser.format_help()}\n{str(e)}"
        raise TypeError(error_msg)

    # Convert the -v and -q counts to a log level
    parsed_vars["log_level"] = min(logging.CRITICAL, max(logging.DEBUG,
        logging.INFO + 10*(parsed_vars.pop("quiet") - parsed_vars.pop("verbose"))
    ))

    # Check for noun and verb
    if parsed_vars["noun"] is None:
        raise TypeError(base_parser.format_help().rstrip())
    elif parsed_vars["verb"] is None:
        noun_parser = noun_parsers[parsed_vars["noun"]]
        raise TypeError(noun_parser.format_help().rstrip())

    # Return the dict representation
    return parsed_vars


def get_logger(name=__name__, level=logging.INFO, fmt="%(message)s"):
    logger = logging.getLogger(name)
    if not logger.hasHandlers():
        printHandler = logging.StreamHandler()
        printHandler.setLevel(level)
        printHandler.setFormatter(logging.Formatter(fmt))
        logger.addHandler(printHandler)
    logger.setLevel(level)
    return logger


def main():
    """
    The main entry point of the htcondor_cli tool.
    Return value will be used as the exit code.
    """

    # Parse arguments
    try:
        options = parse_args()
    except Exception as e:
        logger = get_logger( )
        logger.error(str(e))
        return 2

    # Pop out the noun, verb, and log level from the options
    noun = options.pop("noun")
    verb = options.pop("verb")
    log_level = options.pop("log_level")

    # Set up main logger
    main_logger = get_logger(__name__, level=log_level)

    # Get the requested noun-verb subclass and run it with options
    try:
        main_logger.debug(f"Attempting to run {noun} {verb} with options {options}")
        noun_cls = NOUNS[noun]
        verb_cls = getattr(noun_cls, verb)
        verb_logger = get_logger(f"{__name__}.{noun}.{verb}", level=log_level)
        verb_cls(logger=verb_logger, **options)
    except Exception as e:
        main_logger.debug("Caught exception while running in verbose mode:", exc_info=True)
        main_logger.error(f"Error while trying to run {noun} {verb}:\n{str(e)}")
        return 1
    return 0
