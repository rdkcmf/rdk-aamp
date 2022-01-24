---

# Seetup AAMP anvironment on Ubuntu

This document contains the instructions to setup and debug stand alone AAMP (aamp-cli) on Ubuntu.


**1. Install dependencies

```
bash install-linux-deps.sh
```
This will install additional packages needed by the system to work on this project

**2. Setup additional libraries**

```
bash install-linux.sh
```
This scripts checkouts additional libraries like aampmetrics,aampabr and  gst-plugins-rdk-aamp. 
It also runs intall_libdash script from the main directory

# Clean setup
**1. Run script clean.sh

```
bash clean.sh
```
It will remove directories with additional libraries and also build directory inside aamp.
Could be used f.e when switching to another branch

# Build and execute aamp-cli
**1. Open aamp/ubuntu-aamp-cli.code-workspace VS Code**

**3. Configure cmake project**

```
If required install cmake tools extension to vs code

Open cmake extansion and click "Configure All Projects" button

NOTE: This step could be omitted. You can just click Yes on the popup which appears when you open the workspace: 

'Would you like to configure project 'aamp'?

```

**3. Build the code**

```
In VS Code run Terminal->Run Build Task (or press Ctrl+Shift+B) and select aamp-cli

```

**4. Debugging**

In VS Code Run -> Start debugging (or just press F5)
Debug console will appear where you can enter link to the stream. Gdb is attached to aamp-cli so you can setup breakpoints

