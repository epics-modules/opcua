# Configuration file for the Sphinx documentation builder.

project = 'EPICS OPC UA Device Support'
copyright = '2025, Ralph Lange'
author = 'Ralph Lange'

# -- General configuration ---------------------------------------------------

extensions = [
    'myst_parser',
    'sphinx.ext.napoleon',
    'sphinx_rtd_theme',
]

templates_path = ['_templates']
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

# -- MyST Parser configuration -----------------------------------------------

myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "substitution",
]
