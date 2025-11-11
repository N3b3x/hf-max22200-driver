---
layout: default
title: "Troubleshooting"
description: "Common issues and solutions for HardFOC ESP-IDF CI Tools"
nav_order: 3
parent: "📚 Reference"
permalink: /docs/troubleshooting/
---

# Troubleshooting Guide

This guide helps you resolve common issues with the HardFOC ESP-IDF CI Tools.

## 🔍 Common Issues

### Build Failures

#### ESP-IDF Not Found
**Problem**: Workflow fails with "ESP-IDF not found" error

**Solutions**:
1. **Check ESP-IDF version** in workflow configuration
2. **Verify version availability** in ESP-IDF releases
3. **Update to supported version** (v4.4 or later)

```yaml
# In .github/workflows/build.yml
- name: Build Application
  uses: espressif/esp-idf-ci-action@v1
  with:
    esp_idf_version: "release/v5.5"  # Use supported version
```

#### Configuration Errors
**Problem**: "Configuration file not found" or "Invalid configuration"

**Solutions**:
1. **Verify file exists**: Check `/examples/esp32/app_config.yml`
2. **Validate YAML syntax**:
   ```bash
   yq eval /examples/esp32/app_config.yml
   ```
3. **Check file permissions** and location

#### Build Type Not Supported
**Problem**: "Build type not supported" error

**Solutions**:
1. **Check app configuration** in `app_config.yml`
2. **Verify build types** are defined for the app
3. **Update configuration** to include required build types

### CI/CD Issues

#### Workflow Not Triggering
**Problem**: GitHub Actions workflow doesn't run

**Solutions**:
1. **Check workflow file** is in `.github/workflows/`
2. **Verify YAML syntax** is correct
3. **Check trigger conditions** (branches, paths)
4. **Ensure workflow is enabled** in repository settings

#### Matrix Build Failures
**Problem**: Some matrix combinations fail

**Solutions**:
1. **Check excluded combinations** in workflow
2. **Verify ESP-IDF version compatibility**
3. **Review app-specific requirements**
4. **Add exclusions** for incompatible combinations

### Port Detection Issues

#### No ESP32 Devices Found
**Problem**: Port detection script finds no devices

**Solutions**:
1. **Check USB connection** and drivers
2. **Verify device permissions** on Linux
3. **Try different USB port** or cable
4. **Check device manager** for driver issues

#### Permission Denied
**Problem**: "Permission denied" when accessing serial ports

**Solutions**:
1. **Add user to dialout group** (Linux):
   ```bash
   sudo usermod -a -G dialout $USER
   ```
2. **Check device permissions**:
   ```bash
   ls -la /dev/ttyUSB*
   ```
3. **Restart terminal** after group changes

## 🛠️ Debugging

### Enable Debug Mode

Add debug output to workflows:

```yaml
- name: Debug Information
  run: |
    echo "ESP32_PROJECT_PATH: $ESP32_PROJECT_PATH"
    echo "App: ${{ matrix.app }}"
    echo "Build Type: ${{ matrix.build_type }}"
    echo "ESP-IDF Version: ${{ matrix.idf_version }}"
```

### Check Logs

1. **GitHub Actions logs** - Check workflow execution logs
2. **Build logs** - Look for specific error messages
3. **ESP-IDF logs** - Check ESP-IDF build output

### Local Testing

Test configurations locally:

```bash
# Test build configuration
cd /examples/esp32
./scripts/build_app.sh validate your_app Release

# Test port detection
./scripts/detect_ports.sh

# Test configuration loading
./scripts/config_loader.sh
```

## 📋 Diagnostic Checklist

### Pre-Flight Checks

- [ ] **ESP-IDF installed** and accessible
- [ ] **Project structure** follows expected layout
- [ ] **Configuration files** exist and are valid
- [ ] **Workflow files** are in correct location
- [ ] **GitHub Actions** are enabled for repository

### Build System Checks

- [ ] **app_config.yml** syntax is valid
- [ ] **Applications** are defined correctly
- [ ] **Build types** are supported
- [ ] **ESP-IDF versions** are available
- [ ] **Source files** exist and are accessible

### CI/CD Checks

- [ ] **Workflow syntax** is correct
- [ ] **Matrix strategy** is valid
- [ ] **Secrets** are configured (if needed)
- [ ] **Triggers** are set up correctly
- [ ] **Permissions** are sufficient

## 🆘 Getting Help

### Self-Service Resources

1. **Check this guide** for common issues
2. **Review GitHub Actions logs** for specific errors
3. **Test locally** before pushing to CI
4. **Validate configuration** files

### Community Support

- **[GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues)** - Report bugs and request features
- **[Discussions](https://github.com/n3b3x/hf-espidf-project-tools/discussions)** - Ask questions and share solutions
- **[ESP-IDF Forum](https://esp32.com/)** - ESP-IDF specific issues

### Professional Support

For enterprise or complex issues:
- **Contact maintainers** via GitHub
- **Request consultation** for custom setups
- **Submit feature requests** for specific needs

## 📚 Additional Resources

- **[Build System](/docs/build-system/)** - Detailed build documentation
- **[CI Pipeline](/docs/ci-pipeline/)** - Workflow optimization
- **[Configuration](/docs/configuration/)** - Configuration management
- **[Examples](/docs/examples/)** - Real-world usage examples

---

**Still having issues?** Check our [GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues) or create a new issue with detailed information about your problem.
