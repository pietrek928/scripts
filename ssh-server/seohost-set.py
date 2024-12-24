#!/rw/pyenv/py312/bin/python

from asyncio import run
import re
import sys
from random import choice
from string import ascii_lowercase, digits

from lxml import html
from httpx import AsyncClient, Response

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
        tree = html.fromstring(body)
        token_input = tree.xpath('//input[@name="_token"]')
        if len(token_input) != 1:
            print(f'!!!!!!! found {len(token_input)} tokens')
        if token_input:
            return token_input[0].attrib['value']

    def _update_cookies(self, resp: Response):
        self._cookies.update(dict(
            self._get_cookies(resp.headers)
        ))

    async def _query_token(self, url_suf):
        async with AsyncClient(cookies=self._cookies, headers=dict(
            self._headers, **{
                'Origin': DOMAIN_PREFIX.strip('/'),
                # 'Referer': DOMAIN_PREFIX + referer_suf,
            }
        ), http2=True) as session:
            url = DOMAIN_PREFIX + url_suf
            resp = await session.get(url)
            resp.raise_for_status()
            self._update_cookies(resp)
            return self._get_token(resp.text)

    async def _post(self, url_suf: str, referer_suf: str, data: dict):
        async with AsyncClient(cookies=self._cookies, headers=dict(
            self._headers, **{
                'Origin': DOMAIN_PREFIX.strip('/'),
                'Referer': DOMAIN_PREFIX + referer_suf,
            }
        ), http2=True) as session:
            resp = await session.post(DOMAIN_PREFIX + url_suf, json=data)
            if resp.status_code >= 400:
                resp.raise_for_status()
            self._update_cookies(resp)

    async def login(self, email, password):
        token = await self._query_token('login')
        await self._post('login', 'login', dict(
            _token=token,
            email=email,
            password=password,
            remember='on',
        ))
        return self

    async def update_record(self, zone_id, record_id, name, value, priority=0, ttl=86400, type='A'):
        token = await self._query_token(f'dns/records/{record_id}/edit')
        await self._post(f'dns/records/{record_id}', f'dns/{zone_id}', dict(
            _method='PATCH',
            _token=token,
            record_type=type,
            record_name=name,
            record_prio=priority,
            record_value=value,
            record_ttl=ttl,
        ))
        return self
