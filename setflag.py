#!/usr/bin/python2.7

# setflag script for ctf pizza service

# flag id: random string used as name during order
# flag: random string used as address during order

from __future__ import print_function

import urllib2
from urllib import urlencode
from random import choice
from string import ascii_lowercase
from contextlib import closing # needed for py2.7 httplib to be used with contextmanager


def _rand_str(length=10):
    return ''.join((choice(ascii_lowercase) for _ in range(length)))


def _flag():
    return 'FLAG_'+_rand_str(13)


class SetFlag():

    # POST flag as user flag_id, collect and return token
    def execute(self, ip, port, flag):
        assert isinstance(ip, str)
        assert isinstance(port, int) and (0 < port < 2**16)

        self.flag_id = name = _rand_str()
        self.error_msg = ''
        self.token = ''
        self.error = -1

        post_body = urlencode({
            'address': flag,
            'name': self.flag_id,
            'pizza_id': 1
        })

        post_url = 'http://{}:{}/order'.format(ip, port)

        try:
            with closing(urllib2.urlopen(url=post_url, data=post_body, timeout=20)) as res:
                self.token = res.read()
                if len(self.token.split('.')) != 3:
                    self.error = 1
                    self.error_msg = 'malformed token'
                else:
                    self.error = 0
        except urllib2.HTTPError as e:
            if e.code:
                self.error = 1 # up but nonfunctional
                self.error_msg = '{} {}'.format(e.code, e.reason)
            else: 
                self.error = -1 # down
                self.error_msg = e.reason
        except urllib2.URLError as e:
            self.error = -1
            self.error_msg = e.reason
        except Exception as e:
            self.error = -1
            self.error_msg = str(e)
            

    def result(self):
        return {
            'FLAG_ID': self.flag_id,
            'TOKEN': self.token,
            'ERROR': self.error,
            'ERROR_MSG': self.error_msg
        }


# debug
if __name__ == "__main__":

    ip = '10.40.1.1'
    port = 5101

    sf_obj = SetFlag()
    sf_obj.execute(ip, port, _flag())
    print(sf_obj.result())