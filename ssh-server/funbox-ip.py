#!/usr/bin/python

import requests


class FunboxSession:
    def __init__(self, url):
        self._url = url.strip('/')
        self._headers = {
            'Content-Type': 'application/x-sah-ws-4-call+json',
            'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64; rv:86.0) Gecko/20100101 Firefox/86.0',
            'Authorization': 'X-Sah-Login',
            'Origin': f'{self._url}',
            'Referer': f'{self._url}/',
        }

    def login(self, username, password):
        data = dict(
            service='sah.Device.Information',
            method='createContext',
            parameters=dict(
                applicationName='webui',
                username=username,
                password=password,
            )
        )
        r = requests.post(
            f'{self._url}/ws',
            json=data, headers=self._headers
        )
        r.raise_for_status()

        context_id = r.json()['data']['contextID']
        sessid_cookie = r.headers['Set-Cookie'].split(';')[0]
        self._headers['Cookie'] = f'sah/contextId={context_id}; {sessid_cookie}'
        self._headers['X-Context'] = context_id
        self._headers['Authorization'] = f'X-Sah {context_id}'
        return self

    def get_public_ip(self):
        r = requests.post(
            f'{self._url}/ws',
            json=dict(
                service='NMC',
                method='getWANStatus',
                parameters=dict()
            ), headers=self._headers
        )
        r.raise_for_status()
        return r.json()['data']['IPAddress']
