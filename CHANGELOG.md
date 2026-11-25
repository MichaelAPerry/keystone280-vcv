# Changelog

## 2.0.0

- First public source release, with 14 modules under the `MichaelPerry-Keystone280` plugin slug.
- Original dark panels with separate input and output fields and functional module labels.
- Bench-calibrated DSP with native unit tests.
- Added a panel-by-panel manual with patch examples and calibration notes.
- Added Library links and reviewed module descriptions and browser tags.
- Reject non-finite cable voltages at the Rack inputs before they reach DSP.
- Added a native robustness test for all 14 modules at three sample rates, with single- and eight-instance runs.
- Run the native tests on Ubuntu CI and use Node 24 actions for checkout and artifacts.
