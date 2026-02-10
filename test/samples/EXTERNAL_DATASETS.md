# External Log Datasets

This document lists external log datasets that can be used for testing Duck Hunt parsers.
These datasets are not included in the repository due to size but can be downloaded for local testing.

## Included as Submodules

### GHALogs (GitHub Actions)
- **Location**: `test/samples/ghalogs/`
- **Source**: https://github.com/D2KLab/gha-dataset
- **License**: MIT
- **Content**: 116K GitHub Actions workflows, 513K runs, 2.3M steps
- **Size**: Examples included; full dataset is 140GB on Zenodo

To use the example logs:
```bash
cd test/samples/ghalogs/examples
tar -xzf log.tar.gz -C logs --transform='s/ /_/g'
```

### Loghub
- **Location**: `test/samples/loghub/`
- **Source**: https://github.com/logpai/loghub
- **License**: MIT
- **Content**: System logs (HDFS, Hadoop, Spark, Linux, Android, etc.)

## Available for Download

### LogChunks (Travis CI Build Logs)
- **Source**: https://zenodo.org/records/3632351
- **License**: CC-BY-4.0 (Attribution required)
- **Content**: 797 Travis CI logs from 80 GitHub repos in 29 languages
- **Size**: 24 MB (zip)

To download and use:
```bash
cd test/samples
curl -L -o logchunks.zip "https://zenodo.org/records/3632351/files/LogChunks.zip?download=1"
unzip logchunks.zip
rm logchunks.zip
```

Structure:
- `LogChunks/logs/<language>/<repo>/failed/<build_id>.log` - Raw Travis CI logs
- `LogChunks/build-failure-reason/<language>/<repo>.xml` - Labeled failure chunks

### CIBench (CI Build/Test Selection)
- **Source**: https://zenodo.org/records/4682056
- **License**: Check Zenodo record
- **Content**: CI build and test data for selection/prioritization research

## Citation

When using these datasets for research, please cite the original papers:

**LogChunks**:
```bibtex
@inproceedings{brandt2020logchunks,
  title={LogChunks: A Data Set for Build Log Analysis},
  author={Brandt, Carolin and Panichella, Annibale and Zaidman, Andy and Beller, Moritz},
  booktitle={MSR 2020},
  year={2020}
}
```

**GHALogs**:
```bibtex
@inproceedings{msr25_ghalogs,
  author={Moriconi, Florent and Durieux, Thomas and Falleri, Jean-Rémi and Francillon, Aurélien and Troncy, Raphael},
  title={GHALogs: Large-Scale Dataset of GitHub Actions Runs},
  booktitle={MSR 2025},
  year={2025}
}
```

**Loghub**:
```bibtex
@inproceedings{he2020loghub,
  title={Loghub: A Large Collection of System Log Datasets towards Automated Log Analytics},
  author={He, Shilin and Zhu, Jieming and He, Pinjia and Lyu, Michael R.},
  booktitle={ISSRE 2020},
  year={2020}
}
```
