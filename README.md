# Reproducer: A CCR write clears the CCR fields it does not own

CCR.BFHFNMIGN and CCR.TRD are shared between Security states; the other fields are banked
(Armv8-M ARM DDI0553B.r, D1.2.9). tlib stores the shared fields in the Secure bank's word
(`M_REG_COMMON` is `M_REG_S`) and `ccr_write()` overwrites that word, so with TrustZone a Secure
write clears BFHFNMIGN and TRD, and a Non-secure write clears the Secure banked fields. This
regressed when CCR moved into tlib (`1ae05700`).

`src/test.c` runs on a TrustZone-enabled Cortex-M33 (`test.repl`). Fixed by tlib branch `arm-m-ccr-common-fields`.

On Renode `master`:

```
failed: Firmware Should Finish
    The firmware did not finish its steps
failed: Secure CCR Write Should Keep Banked And Shared Fields
    CCR after setting DIV_0_TRP and BFHFNMIGN: got 0x10, expected 0x110
failed: Non-secure CCR Write Should Keep The Secure CCR
    Secure CCR after writing CCR_NS: got 0x0, expected 0x10
failed: Non-secure CCR Write Of BFHFNMIGN Should Be Ignored Without BFHFNMINS
    CCR.BFHFNMIGN after clearing it through CCR_NS: got 0x0, expected 0x1
OK: Non-secure CCR Write Of BFHFNMIGN Should Take Effect With BFHFNMINS
OK: TRD Should Be RES0 On An Armv8.0-M Core
failed: Bits 9 And 0 Should Be RES1
    CCR bits [9] and [0]: got 0x0, expected 0x201
failed: BFHFNMIGN Should Ignore A BusFault At Negative Priority
    Execution continued after the bus error: got 0x0, expected 0x1
```

Passes with the fix.

---

# Bare-metal Cortex-M33 reproducer harness

Harness for CPU-level reproducers; each reproducer branch adds a `src/test.c` and `test.robot`.

* `src/common/` - startup code, vector table, linker script. Thread mode runs on PSP; every
  exception goes to the test's `on_exception()`, which can abandon the step and continue with the
  next (`resume_next_step()`). Results go to a RAM array at `0x2000F000`.
* `src/test.c` - `test_init()`, the steps, and `on_exception()`.
* `test.repl` - minimal Cortex-M33 (NVIC, MPU, 64 KiB flash and RAM).
* `test.resc`, `common.robot` - load and run the ELF and check the results.
* `build.sh` - builds `src/test.elf` with `arm-none-eabi-gcc` into `artifacts/`.

The example test on this branch passes.

---

# Renode issue reproduction template

This repository is meant to help report issues in (and provide contributions to) the open source [Renode simulation framework](https://renode.io).

It has a GitHub Actions CI set up for you so that you only need to provide the minimum amount of data which reproduces the issue (= makes the CI fail).

Fork this repo and [adapt the template to build a test case which shows the failure](#using-the-template-to-make-reproducible-issue-representation), and if you have an idea how to [provide a fix](#usage-to-report-bugs), implement it in this repo to showcase the desired solution and outcome, all nicely automated.

> [!IMPORTANT]
> Remember to enable workflows on your forked repository, by going to `Actions` tab and clicking `I understand my workflows, go ahead and enable them`, otherwise CI will not be executed when you commit your patches.

## Using the template to make reproducible issue representation

Basic [Renode test script](https://renode.readthedocs.io/en/latest/basic/monitor-syntax.html) is already prepared for you to build upon.
This script creates a single [Nucleo H533RE](https://designer.antmicro.com/library/devices/nucleo_h533re?software=zephyr|nucleo_h533re|) platform and loads Zephyr Hello World binary.
The [Robot Framework](https://robotframework.org/) file can easily [be modified](https://renode.readthedocs.io/en/latest/introduction/testing.html#running-the-robot-test-script) to recreate the scenario which causes the issue.

Feel free to modify both of these files and add other if needed e.g. by including [custom platform files](https://renode.readthedocs.io/en/latest/basic/describing_platforms.html#describing-platforms) or by [adding custom models](#overriding-existing-or-adding-new-peripheral-models-in-runtime).

> [!TIP]
> If you want to run this template locally, you can use [act](https://github.com/nektos/act), which allows you to run CI locally.
Remember to specify `artifact-server-path` with the directory to which artifacts will be uploaded. Otherwise, CI won't run successfully. Enabling `--action-offline-mode` caches Renode compilation speeding up the workflow.
>
> ```
> act --artifact-server-path=artifacts --action-offline-mode
> ```

To change the Renode version which the CI is testing against, change `renode-revision` to specific commit SHA (or tag) in [the workflow file](.github/workflows/test.yml)
If the reproduction requires changes to [Renode's source code](https://renode.readthedocs.io/en/latest/advanced/building_from_sources.html) you can point `renode-repository` to your forked Renode repository in the CI.

Report an issue in [Renode repository](https://github.com/renode/renode) and link to your fork of this repository, with any additional explanations needed.

Files in the `artifacts/` directory will automatically be saved as build artifacts so if you want to share e.g. logs, make sure they end up there after the CI executes.

## How does this template work

1. When you push a commit into your fork of this repository **AND enable workflows**, a GitHub Workflow (CI) is ran.
2. For each of selected Renode revisions
    1. `build.sh` is executed, allowing you to easily install additional dependencies
    2. Custom artifacts located in `artifacts` directory are uploaded to `build-[name]`
    3. Renode is downloaded, which runs your `test.robot` test
    4. Test results are uploaded as `test-results-[name]`

## Important files in this repo

* [test.resc](./test.resc) - Renode script file, this is where you load the platform and set up simulation
* [test.robot](./test.robot) - [Robot Framework](https://robotframework.org/) test file, where you instruct Renode how to reproduce the erroneous behaviour
* [test.yml](./.github/workflows/test.yml#L21) - the GH Actions script, here you can add your fork of Renode to the suite or make fundamental changes to the pipeline
* [artifacts](./artifacts) - any files you want the testing CI to use or store for demonstration (e.g. built binaries, logs) should end up here during the CI job
* [build.sh](./build.sh) - a stub of a potential build script for your [test software](#providing-test-software)
* [requirements.txt](./requirements.txt) - add any Python requirements here, if needed

## Providing fixes

Sometimes you may already know what the fix should be - this repository is an easy way to get your fix included in mainline Renode!

To do that, implement changes in this repo which make the previously failing CI green again. One way to do that may involve providing standalone `.cs` files with the fixed model code and loading them in runtime to override the original implementations.

If you report an issue with such a fix already in place using this repository - we can then easily verify this on our end and help you prepare a PR.

### Overriding existing or adding new peripheral models in runtime

Loading new `.cs` files in runtime is as easy as executing `include @my_file.cs` in the `.resc` script or `ExecuteCommand    include @my_file.cs` in the `.robot` file. Please note that names of the dynamically added classes should not overlap with existing ones, so if you e.g., fix the `ABC_UART` class that is referenced in the `abc.repl` platform you should create the `ABC_UART_Fixed` class and update the `.repl` file to reference `ABC_UART_Fixed` instead of the original `ABC_UART`.

It might happen that during the dynamic compilation you see compilation errors about unknown types. In such case you should use the `EnsureTypeIsLoaded` command. See the example below for details.

```
# in case your implementation references types available in Renode 
# but not recognized by the dynamic compiler use the `EnsureTypeIsLoaded` command;
# this step is only needed if you encounter unreferenced types errors during execution of the `include` command
EnsureTypeIsLoaded "Antmicro.Renode.NameOfTheUnknownType"

# load your fixed implementation of the model
include @ABC_UART_Fixed.cs

# make sure that abc.repl references the ABC_UART_Fixed class instead of ABC_UART
mach create
machine LoadPlatformDescription @abc.repl

[...]
```

## Providing test software

If you can reproduce your problem with one of our demo binaries hosted at dl.antmicro.com or in the [Zephyr Dashboard](https://zephyr-dashboard.renode.io/), feel free to use that.

If you need your specific software to replicate the issue, please try to create the minimum failing binary (preferably in ELF form; for formats without debug symbols, remember to set the Program Counter after loading them).

You can just commit your binary into the repository and use it in the `.resc` script, especially in situations when due to confidentiality you can only share the binary.

If you can share the sources, please include them e.g. in a `src/` directory - and if you have the time to do so, also adapt `build.sh` to build it.
Otherwise you can just commit a binary you compiled yourself corresponding to the sources.

If you can't share your binary or a minimal test case based on it, you can always adapt the CI to pull it from some secret storage with authentication, but that of course may need more work.
