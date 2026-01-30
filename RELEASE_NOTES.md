# EPICS Diode Release Notes

## Release 2.1.0 (2025-01-30)

### New Features
- Performance testing utilities and tests
  - Added `monitor_updates` test utility
- Reordering support
  - Implemented adjacent reordering handling
- Sequence number support
  - Introduced `global_seq_no`
  - Added sequence number unit tests
  - Updated protocol documentation for sequence number handling

### Improvements
- Fixed and improved test harness
- Improved sender override reporting
- Minor logging improvements
- Updated paths in `diode_ioc.cmd`
- Expanded sender/receiver test coverage

### Bug Fixes
- Fixed clang compilation errors
- Removed unused constants
- Removed pvadiode debug prints
- Fixed crash on non-existent pvaccess channels
- Fixed invalid `CODAC_ROOT` path handling


## Release 2.0.1 (2025-09-29)

### Changes
- Open-source release
- Added GitLab CI/CD integration
- Fixed config.hash implementation to be platform agnostic
