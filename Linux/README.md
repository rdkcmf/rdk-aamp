---

# Setup AAMP environment on Ubuntu

This document contains the instructions to setup and debug stand alone AAMP (aamp-cli) on Ubuntu.

# Automated Setup Process

If you have the standalone script install-aamp.sh, run the script
```
bash install-aamp.sh
```

If you pulled aamp from repository, enter aamp folder and run the script
```
bash aamp/install-aamp.sh
```

After running the script, VS Code should have opened.
At this stage you should be asked what compiler to pick. You should pick the newest version of GCC.
Then on the left side of VS Code, You should see the CMake Tools extension. Click that and you should see aamp-cli.
To Run first time, right click aamp-cli in CMake Tools and choose "Run in Terminal".
After that, you can run by typing in terminal from the build folder:
```
./aamp-cli
```

# Manual Setup Process
**1. Install dependencies

Install the following packages
```
sudo apt-get install freeglut3-dev
sudo apt-get install libglew-dev
```
Next
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

---

# Build and execute aamp-cli (Manual Steps)
**1. Open aamp/ubuntu-aamp-cli.code-workspace VS Code**

**3. Configure cmake project**

```
If required install cmake tools extension to vs code

Open cmake extansion and click "Configure All Projects" button

NOTE: This step could be omitted. You can just click Yes on the popup which appears when you open the workspace: 

'Would you like to configure project 'aamp'?

NOTE: At this stage you could be asked what compiler to pick. You should pick the newest version of GCC.

```

**3. Build the code**

```
In VS Code run Terminal->Run Build Task (or press Ctrl+Shift+B) and select aamp-cli

```

**4. Debugging**

In VS Code Run -> Start debugging (or just press F5)
Debug console will appear where you can enter link to the stream. Gdb is attached to aamp-cli so you can setup breakpoints

To set a breakpoint, click on a number that corresponds with your line of code and a red dot should appear beside it. A breakpoint has been set and now when you rerun it will break here.
To see more about VsCode debugging : https://code.visualstudio.com/docs/editor/debugging

