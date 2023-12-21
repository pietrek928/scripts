#!/usr/bin/python

import re
import sys
from random import choice
from string import ascii_lowercase, digits

import requests
from requests import Response

DOMAIN_PREFIX = 'https://panel.seohost.pl/'


class SeohostSession:
    def __init__(self):
        self._cookies = {
            'LaVisitorNew': 'Y',
            'LaVisitorId': self._gen_id(),
            'LaSID': self._gen_id(),
        }
        self._headers = {
            'Host': 'panel.seohost.pl',
            'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64; rv:86.0) Gecko/20100101 Firefox/86.0',
        }

    def _gen_id(self):
        return ''.join(choice(ascii_lowercase + digits) for _ in range(29))

    @staticmethod
    def _get_cookies(headers):
        cookies_s = headers['Set-Cookie']
        for cookie_val in re.findall(r'[-\w]+=[%\w]+; expires', cookies_s):
            cookie_val = cookie_val.split(';')[0].strip()
            yield cookie_val.split('=')

    @staticmethod
    def _get_token(body: str):
        return re.findall(r'name="_token" value="\w+"', body)[0].split('"')[3]

    def _update_cookies(self, resp: Response):
        self._cookies.update(dict(
            self._get_cookies(resp.headers)
        ))

    def _query_token(self, url_suf):
        url = DOMAIN_PREFIX + url_suf
        resp = requests.get(url, cookies=self._cookies, headers=dict(
            self._headers, **{
                'Origin': DOMAIN_PREFIX,
                # 'Referer': DOMAIN_PREFIX + referer_suf,
            }
        ))
        resp.raise_for_status()
        self._update_cookies(resp)
        return self._get_token(resp.text)

    def _post(self, url_suf: str, referer_suf: str, data: dict):
        resp = requests.post(
            DOMAIN_PREFIX + url_suf, cookies=self._cookies, headers=dict(
                self._headers, **{
                    'Origin': DOMAIN_PREFIX,
                    'Referer': DOMAIN_PREFIX + referer_suf,
                }
            ), data=data
        )
        resp.raise_for_status()
        self._update_cookies(resp)

    def login(self, email, password):
        token = self._query_token('login')
        self._post('login', 'login', dict(
            _token=token,
            email=email,
            password=password,
            remember='on',
        ))
        return self

    def update_record(self, zone_id, record_id, name, value, priority=0, ttl=86400, type='A'):
        token = self._query_token(f'dns/{zone_id}/records/{record_id}/edit')
        self._post(f'/dns/records/{record_id}', f'dns/{zone_id}', dict(
            _method='PATCH',
            _token=token,
            record_type=type,
            record_name=name,
            record_prio=priority,
            record_value=value,
            record_ttl=ttl,
        ))
        return self
