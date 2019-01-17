# FSManager

FSManager is an interface providing backup capabilities in this way:

- Provides the portable component ```NVSInterface.h``` which exposes a comon interface for non volatile Key-Value storage backup subsystems.
- ```FSManager``` component is ported to ```arm mbed``` and ```ESP-IDF``` providing different capabilites (FATFilesystem, key-value, etc...) depending on the porting.

---
---
  
## Changelog

---
### **17 Jan 2019**
- [x] Added ```component.mk```
- [x] Removed ```test``` folder
