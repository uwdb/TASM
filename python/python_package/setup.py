import setuptools

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setuptools.setup(
    name="tasm",
    version="0.0.1",
    author="Maureen Daum",
    author_email="mdaum@cs.washington.edu",
    description="TASM video storage manager",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/maureendaum/TASM",
    packages=setuptools.find_packages(),
    package_dir={'tasm': 'tasm'},
    package_data={'tasm': ['*.so']},
    classifiers=[
        "Programming Language :: Python :: 3",
        "Operating System :: OS Independent",
    ],
    python_requires='>=3.6',
)