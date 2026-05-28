"""Helper functionality to test CSH using pexpect"""

import os
import re
from contextlib import contextmanager

import pexpect
import pexpect.expect
import psutil
import pytest
from pexpect.expect import searcher_re


class AnsiStripper(pexpect.expect.Expecter):
    # regex for vt100 from https://stackoverflow.com/a/14693789/5008284
    ansi_escape = re.compile(r"\x1B[@-_][0-?]*[ -/]*[@-~]")

    def new_data(self, data):
        data = self.ansi_escape.sub("", data)
        return pexpect.expect.Expecter.new_data(self, data)


class CSHPexpect(pexpect.spawn):
    def expect_list(
        self,
        pattern_list,
        timeout: float | None = -1.0,
        searchwindowsize: int | None = -1,
        async_=False,
        **kw,
    ):
        if timeout == -1:
            timeout = self.timeout
        exp = AnsiStripper(self, searcher_re(pattern_list), searchwindowsize)
        return exp.expect_loop(timeout)


class CshSession:
    csh_version_re = r"Copyright \(c\) 2016\-(\d\d\d\d).*Compiled: \w\w\w\s+\d\d?\s+\d\d\d\d git: (.*)\r\n"
    prompt_re = f"{os.uname().nodename}  "

    def __init__(self):
        self.session = CSHPexpect("csh", encoding="utf-8", timeout=0.5)
        self.session.expect(self.csh_version_re)
        self.copyright_year = int(self.session.match.groups()[0].strip())
        self.version = self.session.match.groups()[1].strip()

    def prompt(self, timeout=0.5):
        return self.session.expect([self.prompt_re], timeout=timeout)

    def sendline(self, line, csh_prompt=True):
        self.session.sendline(line)
        self.session.expect([r" @ \d\d:\d\d:\d\d d\. \d\d/\d\d/\d\d\r\n", "\n"])
        if csh_prompt:
            self.session.expect([self.prompt_re])

    def get_commands(self):
        self.session.sendline("help")
        self.session.expect([r" @ \d\d:\d\d:\d\d d\. \d\d/\d\d/\d\d\r\n", "\n"])
        self.session.expect([self.prompt_re])
        return [x.strip() for x in self.session.before.strip().split("\n")]

    def __getattr__(self, attr):
        return getattr(self.session, attr)


@pytest.fixture
def csh_session():
    return CshSession()


@pytest.fixture
def csh_session2():
    return CshSession()

@pytest.fixture
@contextmanager
def zmqproxy():    
    pid = os.fork()
    if pid == 0:
        os.execvp("zmqproxy", ["zmqproxy"])
    else:
        try:
            yield pid
        finally:
            for child in psutil.Process(os.getpid()).children(recursive=True):
                child.kill()
