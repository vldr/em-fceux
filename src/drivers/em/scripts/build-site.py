#!/usr/bin/env python
import sys
from publisher import Publisher

if __name__ == '__main__':
	if len(sys.argv) < 3:
		print 'tool to generate a versioned site for em-fceux\n'
		print 'usage: build-site.py <srcdir> <dstdir>\n'
		print 'srcdir - source site tree w/ index.html, generated fceux.* javascript files etc.'
		print 'dstdir - directory to generate the versioned site'
		exit(1)

	templates = ['index.html', 'style.css', 'loader.js']
	replaces = {'fceux.js': ['fceux.js.mem', 'fceux.data', 'fceux.wasm']}
	keep_names = ['index.html', 'gpl-2.0.txt']
	p = Publisher(sys.argv[1], sys.argv[2], templates, replaces, keep_names)
	#p.dry = True
	p.gzip = ['fceux.js', 'fceux.js.mem', 'fceux.data', 'style.css', 'loader.js', 'fceux.wasm']
	p.publish()
