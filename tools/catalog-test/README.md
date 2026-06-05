# Fake search catalog

This directory contains tooling for generating a large SQLite catalog used by the disabled search scrolling
performance test:

```powershell
py -X utf8 tools\catalog-test\create_fake_catalog.py
```

The script uses only the Python standard library, including `sqlite3`, and is intended to run with Python 3.10
or newer. It creates `tools\catalog-test\fake-search-catalog.sqlite`, which is intentionally excluded from the
repository because it is large.

On the current fixture settings the generated database contains 500,000 searchable file rows and is roughly
100 MB. Generation time depends on the machine and disk, but it is normally on the order of seconds to a few
minutes.

Run the performance test explicitly when the fixture exists:

```powershell
x64\Debug\UnitTests.exe --gtest_filter=DISABLED_SearchPaneUiPerformance.SearchAndScrollFakeCatalog --gtest_also_run_disabled_tests
```
