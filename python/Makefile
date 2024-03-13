SPHINX_BUILD  = sphinx-build
DOC_SOURCE    = docs
DOC_BUILD     = docs
INDEXMD       = $(DOC_BUILD)/markdown/index.md

PYTHON        = python
SETUP_PY      = setup.py
DIST          = dist

.PHONY: all dist markdown

all: markdown dist
	@echo "Everything done!"
	@echo '  > Run "twine upload dist/*" to upload to pypi.org.'

markdown:
	@echo "Creating README.md from docstrings."
	@$(SPHINX_BUILD) -M markdown "$(DOC_SOURCE)" "$(DOC_BUILD)" 1>/dev/null
	@cp $(INDEXMD) ./README.md

dist:
	@echo "Creating python distribution."
	@rm -rf $(DIST)
	@$(PYTHON) $(SETUP_PY) sdist bdist_wheel 1>/dev/null
