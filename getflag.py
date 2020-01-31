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
from json import loads
from base64 import b64decode

def _rand_str(length=10):
    return ''.join((choice(ascii_lowercase) for _ in range(length)))


def _flag():
    return 'FLAG_' + _rand_str(13)


class GetFlag():

    # GET flag as user flag_id using token collected by set_flag()
    def execute(self, ip, port, flag_id, token):
        assert isinstance(ip, str)
        assert isinstance(port, int) and (0 < port < 2 ** 16)
        assert isinstance(flag_id, str)
        assert isinstance(token, str)

        _ = flag_id # unused

        self.error_msg = ''
        self.flag = ''
        self.error = -1s

        try:
            data_encoded = token.split('.')[1]
            # no b64 padding in jwt spec, but need to add beacause b64decode() complains otherwise
            data_encoded += "=" * ((4 - len(data_encoded) % 4) % 4)
            token_data = loads(b64decode(data_encoded))
            get_url = 'http://{}:{}/receipt?order_id={}'.format(ip, port, token_data['aud'])
        except Exception as e:
            self.error = 1 # nonfunctional
            self.error_msg = 'malformed token'
            return
        
        try:
            req = urllib2.Request(url=get_url)
            req.add_header('Authorization',  'Bearer ' + token)
            with closing(urllib2.urlopen(req, timeout=25)) as res:
                data = res.read()
        except urllib2.HTTPError as e:
            if e.code:
                self.error = 1 # up but nonfunctional
                self.error_msg = '{} {}'.format(e.code, e.reason)
            else: 
                self.error = -1 # down
                self.error_msg = e.reason
            return
        except urllib2.URLError as e:
            self.error = -1
            self.error_msg = e.reason
            return
        except Exception as e:
            self.error = -1
            self.error_msg = str(e)
            return

        try:
            data = str(data)
            self.flag = loads(data)['address']
            self.error = 0 # functional
        except Exception as e:
            print(str(e))
            self.error = 1
            self.error_msg = 'error deserializing json in response'

    def result(self):
        return {
            'FLAG': self.flag,
            'ERROR': self.error,
            'ERROR_MSG': self.error_msg
        }


if __name__ == '__main__':
    from sys import argv

    gf_obj = GetFlag()
    gf_obj.execute('127.0.0.1', 7777, argv[1], argv[2])
    print(gf_obj.result())