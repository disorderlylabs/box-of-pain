#!/bin/bash
pushd tmp/
m4 out.m4 > out.dot && dot -Tpdf -o out.pdf out.dot
mv out.pdf ..

