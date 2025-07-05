# HACS Setup Guide

This guide helps you prepare the TsuryPhone integration for HACS distribution.

## 📋 Prerequisites

1. **GitHub Repository**: Create a public GitHub repository
2. **Proper Structure**: Ensure integration files are in `home_assistant/custom_components/tsuryphone/`
3. **HACS Validation**: Integration must pass HACS validation checks

## 🚀 Setup Steps

### 1. Repository Setup

1. **Push to GitHub**:
   ```bash
   git remote add origin https://github.com/Tsury/TsuryPhone.git
   git push -u origin main
   ```

2. **Create Release**: 
   - Go to GitHub → Releases → Create a new release
   - Tag: `v1.0.0`
   - Title: `Initial Release`
   - Description: Add release notes

### 2. HACS Integration

#### Option A: Custom Repository (Immediate)
Users can add immediately:
1. HACS → Integrations → ⋮ → Custom repositories
2. Repository: `https://github.com/Tsury/TsuryPhone`
3. Category: Integration

#### Option B: HACS Default (Recommended)
Submit for inclusion in HACS default repositories:
1. Fork [HACS/default](https://github.com/hacs/default)
2. Add entry to `integrations.json`:
   ```json
   {
     "tsuryphone": {
       "name": "TsuryPhone",
       "domain": "tsuryphone"
     }
   }
   ```
3. Create pull request

### 3. Version Management

**Update version** for new releases:
```bash
python scripts/update_version.py 1.0.1
git commit -am "Bump version to 1.0.1"
git tag v1.0.1
git push --tags
```

**GitHub Actions** will automatically:
- Validate the integration
- Create release artifacts
- Update HACS compatibility

## 📁 File Structure

Your repository should have:
```
├── hacs.json                 # HACS configuration
├── info.md                   # HACS description
├── README.md                 # Main project README
├── home_assistant/
│   └── custom_components/
│       └── tsuryphone/       # Integration files
│           ├── __init__.py
│           ├── config_flow.py
│           ├── const.py
│           ├── manifest.json
│           └── ...
├── .github/workflows/        # GitHub Actions
└── scripts/                  # Utility scripts
```

## ✅ HACS Requirements Checklist

- [ ] **Repository**: Public GitHub repository
- [ ] **Manifest**: Valid `manifest.json` with required fields
- [ ] **Documentation**: README.md with installation instructions
- [ ] **Validation**: Passes HACS validation checks
- [ ] **Releases**: Tagged releases for version management
- [ ] **License**: Open source license (MIT recommended)

## 🎯 User Experience

Once set up, users can install with:

1. **HACS → Integrations**
2. **Search "TsuryPhone"** or add custom repository
3. **Click Download**
4. **Restart HA**
5. **Add Integration** via UI

## 🔧 Maintenance

- **Regular updates**: Keep integration compatible with HA versions
- **Release notes**: Document changes in GitHub releases
- **Issues**: Respond to user issues and feature requests
- **HACS compliance**: Maintain HACS validation passing

---

**Ready to publish?** Push to GitHub, create a release, and share with the community! 🎉
