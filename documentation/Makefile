PYTHON?=python3

# make sure that you have doxygen, Sphinx, andsphinx-rtd-theme installed, e.g.: ::
#
#   apt-get install doxygen python3-sphinx python3-sphinx-rtd-theme
#
# Sphinx v6 or newer is recommended,
# get it with 'pip install -U sphinx sphinx-rtd-theme' if older version is installed using apt-get)

all: gen

clean:
	rm -f epics-diode.tag
	rm -rf html latex

gen:
	#doxygen
	$(PYTHON) -m sphinx -j auto -b html . html

commit: gen
	./commit-gh.sh documentation/html/ html/.nojekyll html/*.* html/_*/*.*

.PHONY: all clean gen commit