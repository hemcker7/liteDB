# Changelog

## v0.2.0

### Added

- Page-based storage engine
- Pager
- Binary serialization
- Page headers
- Unit tests
- Stress tests

### Changed

- Table no longer stores std::vector<Row>.
- Storage is now page-oriented.

### Tested

- Serialization
- Pager
- Table

Stress tested with 100,000 rows.