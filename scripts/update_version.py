#!/usr/bin/env python3
"""Update version in manifest.json"""

import json
import sys
import os

def update_version(new_version):
    """Update the version in manifest.json"""
    manifest_path = "home_assistant/custom_components/tsuryphone/manifest.json"
    
    if not os.path.exists(manifest_path):
        print(f"Error: {manifest_path} not found")
        return False
    
    try:
        with open(manifest_path, 'r') as f:
            manifest = json.load(f)
        
        manifest['version'] = new_version
        
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)
        
        print(f"Updated version to {new_version}")
        return True
        
    except Exception as e:
        print(f"Error updating version: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python update_version.py <version>")
        print("Example: python update_version.py 1.0.1")
        sys.exit(1)
    
    version = sys.argv[1]
    if not update_version(version):
        sys.exit(1)
