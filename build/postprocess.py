#!/usr/bin/env python3
"""Finishes the compiled CSS and Bootstrap's JavaScript for Drupal.

  postprocess.py css IN OUT     Bootstrap's custom properties get the --bs-
                                prefix (as in Bootstrap's own dist files, so
                                its docs apply); .menu becomes .bs-menu.
  postprocess.py js IN OUT      .menu becomes .bs-menu in Bootstrap's
                                selectors (string literals only).

Why .bs-menu: Bootstrap 6's dropdown is .menu { display: none }, and every
Drupal menu is a <ul class="menu">. Bootstrap's markup in this theme uses
class="bs-menu" for dropdowns.

Fails (exit 1) when a step changes less than expected, e.g. after a
Bootstrap update moved things: check the output before shipping it.
"""
import re
import sys

MENU = re.compile(r'(?<![\w.-])\.menu(?![\w-])')


def css(text):
    # Custom properties: --name -> --bs-name (the theme's own use --bsn-).
    text, props = re.subn(r'(?<![\w-])--(?!bs-|bsn-)([a-zA-Z_][\w-]*)', r'--bs-\1', text)
    text, menus = MENU.subn('.bs-menu', text)
    if props < 1000 or menus < 10:
        sys.exit('css: only %d custom properties and %d .menu selectors changed' % (props, menus))
    return text, 'css: %d custom properties prefixed, %d .menu -> .bs-menu' % (props, menus)


def js(text):
    # Inside strings only: after a quote, "(", " ", ">" or "," (none of
    # which can come before ".menu" in JavaScript code itself, where it
    # would be a property like config.menu).
    text, count = re.subn(r'(?<=["\'`( >,])\.menu(?![\w-])', '.bs-menu', text)
    if count < 20:
        sys.exit('js: only %d .menu selectors changed' % count)
    return text, 'js: %d .menu -> .bs-menu' % count


if __name__ == '__main__':
    kind, src, dst = sys.argv[1:4]
    with open(src, encoding='utf-8') as f:
        out, report = {'css': css, 'js': js}[kind](f.read())
    with open(dst, 'w', encoding='utf-8') as f:
        f.write(out)
    print(report)
