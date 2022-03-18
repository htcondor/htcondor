"""Common functions for BLAH python scripts"""

import os
import glob
import sys
try:  # for Python 2
    from ConfigParser import RawConfigParser
    from StringIO import StringIO
except ImportError:  # for Python 3
    from configparser import RawConfigParser
    from io import StringIO

class BlahConfigParser(RawConfigParser, object):

    def __init__(self, path=None, defaults=None):
        if path is None:
            if "BLAHPD_CONFIG_LOCATION" in os.environ:
                path = os.environ['BLAHPD_CONFIG_LOCATION']
            elif "BLAHPD_LOCATION" in os.environ and os.path.isfile("%s/etc/blah.config" % os.environ['BLAHPD_LOCATION']):
                path = "%s/etc/blah.config" % os.environ['BLAHPD_LOCATION']
            elif "GLITE_LOCATION" in os.environ and os.path.isfile("%s/etc/blah.config" % os.environ['GLITE_LOCATION']):
                path = "%s/etc/blah.config" % os.environ['GLITE_LOCATION']
            else:
                path = "/etc/blah.config"
        # RawConfigParser requires ini-style [section headers] but since
        # blah.config is also used as a shell script we need to fake one
        config_dir = path + ".d/"
        config_list = sorted(filter(os.path.isfile, glob.glob(config_dir + "*")))
        config_list.insert(0, path)
        user_config = os.path.expanduser("~/.blah/user.config")
        if os.path.isfile(user_config):
            config_list.append(user_config)
        self.header = 'blahp'
        config = "[%s]\n" % (self.header)
        for file in config_list:
            with open(file) as f:
                config += f.read() + "\n"
        vfile = StringIO('[%s]\n%s' % (self.header, config))

        # In Python 2, there is no 'strict' option, but it behaves like
        # strict=False.
        if sys.version_info[0] == 2:
            super(BlahConfigParser, self).__init__(defaults=defaults)
        else:
            super(BlahConfigParser, self).__init__(defaults=defaults, strict=False)
        # TODO: readfp() is replaced by read_file() in Python 3.2+
        self.readfp(vfile)

    def items(self):
        return super(BlahConfigParser, self).items(self.header)

    def get(self, option):
        # ConfigParser happily includes quotes in value strings, which we
        # happily allow in /etc/blah.config. This causes failures when joining
        # paths, for example.
        return super(BlahConfigParser, self).get(self.header, option).strip('"\'')

    def set(self, option, value):
        return super(BlahConfigParser, self).set(self.header, option, value)

    def has_option(self, option):
        return super(BlahConfigParser, self).has_option(self.header, option)
