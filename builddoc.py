#!python3
try:
    from docutils.core import publish_cmdline
except ImportError:
    publish_cmdline = None
import os.path
import sys


def main():
    if publish_cmdline is None:
        sys.stderr.write(
            'Unable to produce documentation: docutils not found.\n')
        rc = 1
    else:
        docs = 'docs'
        infile = os.path.join(docs, 'launcher.rst')
        outfile = os.path.join(docs, 'launcher.html')
        try:
            publish_cmdline(writer_name='html',
                            argv=[infile, outfile])
            rc = 0
        except Exception:
            e = sys.exc_info()[1]
            sys.stderr.write('Failed when producing documentation: %s\n',
                             e)
            rc = 2
    return rc


if __name__ == '__main__':
    sys.exit(main())
