# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
SPHINXPROJ    = BareosMainReference
SOURCEDIR     = source
BUILDDIR      = BareosMainReference


all:
	mv  file:///usr/src/packages/SOURCES/sphinxcontrib-versioning-2.2.1.tar.gz_ file:///usr/src/packages/SOURCES/sphinxcontrib-versioning-2.2.1.tar.gz
	pip install file:///usr/src/packages/SOURCES/sphinxcontrib-versioning-2.2.1.tar.gz_
	git clone git://10.20.40.13/bareos
	cd bareos/docs/manuals/en/new_main_reference/source &&  ./convert_all_tex_files_to_rst.sh && cd -
	sphinx-versioning -l bareos/docs/manuals/en/new_main_reference/source/conf.py  build bareos/docs/manuals/en/new_main_reference/source/ BareosMainReference/
	#@$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	#@$(SPHINXBUILD) -M latexpdf "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

check:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)


.PHONY: help Makefile

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
