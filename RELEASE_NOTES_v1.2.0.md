# Duck Hunt v1.2.0 Release Notes

**90 log formats** across 15 categories | **Schema V2** with generic hierarchy | **Modular parser architecture**

This release brings major improvements to Duck Hunt including Schema V2 with domain-agnostic hierarchy fields, 25+ new parsers for application logging and infrastructure, a complete internal refactor to a modular parser architecture, and critical bug fixes.

---

## Breaking Changes

### Schema V2: Generic Hierarchy Fields

The schema has been redesigned with generic hierarchy fields that work across all domains (CI/CD, Kubernetes, cloud audit, tests). This is a **breaking change** for existing queries.

**Key renames:**
| Old Field | New Field |
|-----------|-----------|
| `workflow_name` | `scope` |
| `job_name` | `group` |
| `step_name` | `unit` |
| `error_fingerprint` | `fingerprint` |
| `workflow_run_id` | `scope_id` |
| `job_id` | `group_id` |
| `step_id` | `unit_id` |

**Removed fields:** `source_file`, `build_id`, `environment`, `file_index`, `root_cause_category`, `completed_at`, `duration`

**New fields:** `target`, `actor_type`, `external_id`, `subunit`, `subunit_id`

See [Migration Guide](docs/migration-guide.md) for detailed migration examples and compatibility view patterns.

---

## New Features

### Application Logging Parsers (10 formats)

Full support for structured application logs across major languages and frameworks:

**Python/Java**
- `python_logging` - Python standard library logging
- `log4j` - Apache Log4j (Java)
- `logrus` - Go structured logging

**Node.js**
- `winston` - Popular Node.js logging
- `pino` - Fast JSON logger
- `bunyan` - Bunyan JSON logs

**.NET**
- `serilog` - .NET structured logging
- `nlog` - NLog framework

**Ruby**
- `ruby_logger` - Ruby stdlib Logger
- `rails_log` - Rails request logs

### Infrastructure Log Parsers (8 formats)

Parse firewall, audit, and container orchestration logs:

**Firewall Logs**
- `iptables` - Linux iptables
- `pf` - BSD Packet Filter
- `cisco_asa` - Cisco ASA firewall
- `vpc_flow` - AWS VPC Flow Logs

**Container & Cloud**
- `kubernetes` - K8s events and pod logs
- `windows_event` - Windows Event Log
- `auditd` - Linux audit daemon
- `s3_access` - S3 access logs

### Cross-Language Structured Log Parsers

- `jsonl` / `ndjson` - JSON Lines format (universal)
- `logfmt` - Key=value format (Go ecosystem)

### System Tracing

- `strace` - Linux system call tracing with syscall extraction, timing, and error analysis

### Generic Format Support

- `regexp` - Custom regex pattern parser for non-standard formats

---

## Bug Fixes

### Critical: Segfault on Auto-Detection

Fixed catastrophic regex backtracking that caused segfaults when auto-detecting certain log formats. The issue occurred with patterns containing nested quantifiers that could trigger exponential backtracking on malformed input.

---

## Internal Improvements

### Modular Parser Architecture

Complete refactor of the parser system from a monolithic registry to a category-based modular architecture:

- **15 parser categories** organized in `src/parsers/`:
  - `test_frameworks/` - pytest, junit, rspec, mocha, gtest, nunit, xunit, duckdb
  - `build_systems/` - make, maven, gradle, msbuild, cargo, cmake, bazel, node, python
  - `linting_tools/` - eslint, mypy, flake8, black, pylint, clang-tidy, etc.
  - `tool_outputs/` - JSON output parsers for various tools
  - `app_logging/` - Application framework loggers
  - `infrastructure/` - Firewall, audit, and cloud logs
  - `ci_systems/` - GitHub CLI, Drone CI, Terraform
  - `cloud_logs/` - AWS CloudTrail, GCP Cloud Logging, Azure Activity
  - `web_access/` - Apache, Nginx access logs, syslog
  - `structured_logs/` - JSONL, logfmt
  - `specialized/` - Valgrind, GDB/LLDB, coverage
  - `workflow_engines/` - GitHub Actions, GitLab CI, Jenkins, Docker

- **Auto-registration**: Each category has an `init.cpp` that registers parsers on extension load
- **Clean separation**: No more monolithic switch statements or manual registration

### Code Cleanup

- Removed ~700 lines of dead legacy registry code
- Eliminated dual registration pattern (parsers no longer register in two places)
- Removed unused files: `legacy_parser_registry.cpp/hpp`, `format_detector.cpp/hpp`
- Simplified `read_duck_hunt_log_function.cpp` from 3000+ lines of switch statements

---

## Statistics

- **201 files changed**, +14,689 / -4,542 lines
- **90 total formats** (up from 65 in v1.1.0)
- **15 parser categories**
- **19 commits** since v1.1.0

---

## Upgrade Path

1. **Update queries** for renamed fields (see migration guide)
2. **Test auto-detection** - behavior improved but some edge cases may detect differently
3. **Review new formats** - consider if new parsers better match your logs

```sql
-- Check available formats
SELECT * FROM duck_hunt_formats() ORDER BY category, format;

-- Example: Use new app logging parser
SELECT severity, message, principal, origin
FROM read_duck_hunt_log('app.log', 'python_logging')
WHERE severity = 'error';
```

---

## Full Changelog

```
ec381a7 Remove legacy parser registry and dead code
4d54c36 Remove legacy parser registration from individual parser files
f6153a2 Fix catastrophic regex backtracking causing segfault on auto-detection
c8e4779 Fix test expectations for sample_files.test
1379b99 Add parser documentation and sample file tests
ef45969 Rename new_parser_registry to parser_registry and clean up includes
d31e2ea Remove dead inline parsers and fix registry initialization
4cbce0d Extract inline parsers to modular files
78e2767 Complete parser category migrations to new registry
75523dc Migrate test_frameworks, build_systems, linting_tools, tool_outputs parsers
dab6af7 Add dispatch function for gradual parser migration
29f6301 Add new modular parser registry architecture
f038a6f Add strace system call trace parser
8624388 Add Schema V2 migration guide
a3d24f0 Implement Schema V2 with generic hierarchy fields
674d417 Add application logging parsers for Node.js, .NET, and Ruby
bdf98c8 Document infrastructure log format schema mappings
dbd0b92 Add infrastructure log parsers (firewall, K8s, audit, cloud)
5180e5d Add application logging parsers: Python, Log4j, Logrus
```
