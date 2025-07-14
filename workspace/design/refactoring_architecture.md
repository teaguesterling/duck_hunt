# Duck Hunt Refactoring Architecture Design

## 🎯 Design Goals

1. **Modularity**: Each parser is a self-contained module
2. **Extensibility**: Adding new formats requires minimal changes
3. **Testability**: Individual parsers can be tested in isolation  
4. **Performance**: Efficient format detection with early termination
5. **Maintainability**: Clear separation of concerns and responsibilities

## 🏗️ Proposed Architecture

### Core Components

```
src/
├── core/
│   ├── parser_registry.hpp          # Central registry for all parsers
│   ├── parser_registry.cpp
│   ├── format_detector.hpp          # Optimized detection engine
│   ├── format_detector.cpp
│   ├── validation_event.hpp         # Event types and utilities
│   └── validation_event.cpp
├── parsers/
│   ├── base/
│   │   ├── parser_interface.hpp     # Base parser interface
│   │   ├── regex_parser.hpp         # Common regex utilities
│   │   └── json_parser.hpp          # Common JSON utilities
│   ├── test_frameworks/             # Test framework parsers
│   │   ├── pytest_parser.hpp
│   │   ├── pytest_parser.cpp
│   │   ├── junit_parser.hpp
│   │   ├── junit_parser.cpp
│   │   └── ...
│   ├── linting_tools/               # Linting tool parsers
│   │   ├── flake8_parser.hpp
│   │   ├── flake8_parser.cpp
│   │   ├── mypy_parser.hpp
│   │   └── ...
│   ├── build_systems/               # Build system parsers
│   │   ├── cmake_parser.hpp
│   │   ├── cmake_parser.cpp
│   │   ├── make_parser.hpp
│   │   └── ...
│   └── ci_systems/                  # CI/CD system parsers
│       ├── github_actions_parser.hpp
│       └── ...
├── intelligence/                    # Phase 3B error intelligence
│   ├── error_fingerprinting.hpp
│   ├── error_fingerprinting.cpp
│   ├── pattern_detection.hpp
│   ├── pattern_detection.cpp
│   ├── similarity_analysis.hpp
│   └── similarity_analysis.cpp
├── multi_file/                      # Phase 3A multi-file processing
│   ├── glob_processor.hpp
│   ├── glob_processor.cpp
│   ├── pipeline_analyzer.hpp
│   └── pipeline_analyzer.cpp
└── table_functions/                 # DuckDB integration
    ├── read_test_results_function.hpp
    ├── read_test_results_function.cpp  # Now just orchestration
    └── parse_test_results_function.cpp
```

### Key Design Patterns

#### 1. **Parser Interface Pattern**
```cpp
class IParser {
public:
    virtual ~IParser() = default;
    virtual bool canParse(const std::string& content) const = 0;
    virtual std::vector<ValidationEvent> parse(const std::string& content) const = 0;
    virtual TestResultFormat getFormat() const = 0;
    virtual std::string getName() const = 0;
    virtual int getPriority() const = 0;  // For detection ordering
};
```

#### 2. **Registry Pattern**
```cpp
class ParserRegistry {
public:
    void registerParser(std::unique_ptr<IParser> parser);
    IParser* findParser(const std::string& content) const;
    std::vector<IParser*> getAllParsers() const;
    
private:
    std::vector<std::unique_ptr<IParser>> parsers_;
    // Sorted by priority for efficient detection
};
```

#### 3. **Detection Engine**
```cpp
class FormatDetector {
public:
    TestResultFormat detectFormat(const std::string& content) const;
    
private:
    const ParserRegistry& registry_;
    // Optimized detection with early termination
    // Pattern caching for repeated content
};
```

## 🔄 Migration Strategy

### Phase 1: Foundation (Week 1)
1. Create new directory structure
2. Extract ValidationEvent types to `core/`
3. Implement base parser interface
4. Create parser registry system
5. Build new format detector

### Phase 2: Parser Extraction (Week 2-3)
1. Extract 5-10 parsers per day
2. Start with simplest parsers (JSON formats)
3. Move to complex parsers (build systems)
4. Maintain 100% test coverage throughout

### Phase 3: Intelligence Modules (Week 4)
1. Extract Phase 3B error intelligence features
2. Create dedicated modules for fingerprinting
3. Separate pattern detection logic
4. Build similarity analysis engine

### Phase 4: Integration & Optimization (Week 5)
1. Update table function implementations
2. Optimize detection performance
3. Add comprehensive integration tests
4. Performance benchmarking

### Phase 5: Advanced Features (Week 6)
1. Plugin architecture for external parsers
2. Configuration system
3. Parser performance metrics
4. Hot-swappable parser registration

## 📊 Expected Benefits

### Maintainability
- **-95% file size**: From 14K lines to ~200 lines per module
- **Clear ownership**: Each parser owns its logic and tests
- **Easy debugging**: Issues isolated to specific modules

### Performance
- **Faster detection**: O(log n) with priority ordering
- **Parallel parsing**: Multiple files can use different parsers
- **Memory efficiency**: Load only needed parsers

### Extensibility
- **New formats**: Add parser + register, done!
- **Plugin system**: External parsers via shared libraries
- **A/B testing**: Multiple parser versions side-by-side

### Testability
- **Unit tests**: Each parser tested independently
- **Integration tests**: Registry and detection tested separately
- **Mocking**: Easy to mock parsers for table function tests

## 🎭 Example: Pytest Parser

```cpp
// parsers/test_frameworks/pytest_parser.hpp
class PytestParser : public IParser {
public:
    bool canParse(const std::string& content) const override {
        return content.find("::") != std::string::npos && 
               (content.find("PASSED") != std::string::npos ||
                content.find("FAILED") != std::string::npos ||
                content.find("SKIPPED") != std::string::npos);
    }
    
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::PYTEST_TEXT; }
    std::string getName() const override { return "pytest"; }
    int getPriority() const override { return 100; }  // High priority
};
```

This transforms our 14K-line monolith into a clean, modular, extensible architecture! 🚀

## Next Steps

Should we start with Phase 1 and build the foundation? I can help implement:
1. The new directory structure
2. Base interfaces and registry
3. Extract the first few parsers as proof of concept

What do you think of this architecture design?