tcl-opencl
=====

This project is a Tcl extension for OpenCL,
and a [VecTcl](https://auriocus.github.io/VecTcl/) extension.

This extension is to re-use VecTcl's NumArray type to handle data
to and from device memory. Thanks for VecTcl's effort.

OpenCL defines an Installable Client Driver (ICD) mechanism to allow
developers to build applications against an Installable Client Driver
loader (ICD loader) rather than linking their applications against a
specific OpenCL implementation. So if your ICD loader is correctly
configured to load a specific OpenCL implementation, your program will
be able to use it.

I test OpenCL functions by using [ocl-icd](https://github.com/OCL-dev/ocl-icd)
and [PoCL](http://portablecl.org/).


Implement commands
=====

### Platform

::opencl::getPlatformID ?number?  
platformIDHandle info param_name  
platformIDHandle close  

`param_name` value should be -  
profile, version, name, vendor, extensions  


### Device

::opencl::getDeviceID platformID type ?number?  
getDeviceIDHandle info param_name  
getDeviceIDHandle close  

`type` value shoue be -  
default, cpu, gpu, accelerator, all  

`param_name` value should be -  
name, profile, vendor, device_version, driver_version, extensions, avaiable,
compiler_avaiable, endian_little, error_correction_support, image_support,
device_type, address_bits, global_mem_cacheline_size, max_clock_frequency,
max_compute_units, max_constant_args, max_read_image_args, max_samplers,
max_work_item_dimensions, max_write_image_args, mem_base_addr_align,
min_data_type_align_size, preferred_vector_with_char, preferred_vector_with_short,
preferred_vector_with_int, preferred_vector_with_long, preferred_vector_with_float,
preferred_vector_with_double, vendor_id, global_mem_cache_size,
global_mem_size, local_mem_size, constant_buffer_size, max_mem_alloc_size,
image2d_max_height, image2d_max_width, image3d_max_depth, image3d_max_height,
image3d_max_width, max_parameter_size, max_work_group_size, 
profiling_timer_resolution, global_mem_cache_type, local_mem_type,
max_work_item_sizes  


### Context

::opencl::createContext deviceID platformID  
contextHandle close  


### CommandQueue

::opencl::createCommandQueue context deviceID  
commandQueueHandle enqueueWriteBuffer buffer numarray  
commandQueueHandle enqueueReadBuffer buffer  
commandQueueHandle enqueueCopyBuffer srcBuffer dstBuffer  
commandQueueHandle enqueueNDRangeKernel kernel global_work_size local_work_size  
commandQueueHandle flush  
commandQueueHandle finish  
commandQueueHandle close  


### Program

::opencl::createProgramWithSource context source  
::opencl::createProgramWithBinary context deviceID binary  
programHandle build  
programHandle info property  
programHandle close  

`property` value should be -  
binary_sizes, binaries  


### Kernel

::opencl::createKernel program kernel_name  
kernelHandle setKernelArg index type value  
kernelHandle close  

`type` value should be -  
mem, int, long, double


### Memory Object

::opencl::createBuffer context flags type length  
bufferHandle close  

`flags` value should be -  
r, w, rw  

`type` value should be -  
int, long, double


Examples
=====

Get basic info -

    package require opencl
    set number [::opencl::getPlatformID]
    for {set i 1} {$i <= $number} {incr i} {
        set platform [::opencl::getPlatformID $i]
        puts "Platform $i"
        puts "\tName: [$platform info name]"
        puts "\tVersion: [$platform info version]"
        puts "\tVendor: [$platform info vendor]"
        puts "\tProfile: [$platform info profile]"

        set number2 [::opencl::getDeviceID $platform all]
        for {set j 1} {$j <= $number2} {incr j} {
            set device [::opencl::getDeviceID $platform all $j]
            puts "\tDevice $j"
            puts "\t\tAvaiable: [$device info avaiable]"
            puts "\t\tName: [$device info name]"
            puts "\t\tVendor: [$device info vendor]"
            puts "\t\tDevice versoin: [$device info device_version]"
            puts "\t\tDevice type: [$device info device_type]"
            puts "\t\tDriver versoin: [$device info driver_version]"

            $device close
        }

        $platform close
    }

Execute a function -

    package require vectcl
    package require opencl

    set code {__kernel void multiply_by(__global int* A, const int c) {
    A[get_global_id(0)] = c * A[get_global_id(0)];
    }
    }

    proc factorial {n} {
        if {$n <= 1} {
            return 1;
        } else {
            return [expr $n * [factorial [expr $n - 1]]]
        }
    }

    try {
        set N 1024
        set c_max 5
        set coeff [factorial $c_max]
        set A [numarray::int [numarray::constfill 1 $N]]
        set B [numarray::int [numarray::constfill 1 $N]]
        set C [numarray::int [numarray::constfill 1 $N]]

        for {set i 0} {$i < $N} {incr i} {
            numarray set A $i $i
            numarray set C $i [expr $i * $coeff]
        }

        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]
        set context [opencl::createContext $device $platform]
        set queue [opencl::createCommandQueue $context $device]
        set program [opencl::createProgramWithSource $context $code]
        $program build

        set kernel [opencl::createKernel $program "multiply_by"]

        set buffer1 [opencl::createBuffer $context rw int $N]

        $queue enqueueWriteBuffer $buffer1 $A

        $kernel setKernelArg 0 mem $buffer1

        for {set c 2} {$c <= $c_max} {incr c} {
            $kernel setKernelArg 1 int $c
            $queue enqueueNDRangeKernel $kernel $N $N
        }
        $queue finish

        set B [$queue enqueueReadBuffer $buffer1]
        $queue finish

        set success 1
        for {set i 0} {$i < $N} {incr i} {
            set clvalue [numarray::get $B $i]
            set ckvalue [numarray::get $C $i]
            if {$clvalue != $ckvalue} {
                set success 0
                break
            }
        }

        if {$success == 1} {
            puts "Arrays are equal!"
        } else {
            puts "Arrays are NOT equal!"
        }

        $buffer1 close
        $queue close
        $kernel close
        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }

Execute a function -

    package require vectcl
    package require opencl

    set code {__kernel void vector_add(__global const double *A, __global const double *B, __global double *C) {
        int i = get_global_id(0);
        C[i] = A[i] + B[i];
    }
    }

    try {
        set A [numarray::constfill 5.0 1024]
        set B [numarray::constfill 10.0 1024]

        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]
        set context [opencl::createContext $device $platform]
        set queue [opencl::createCommandQueue $context $device]
        set program [opencl::createProgramWithSource $context $code]
        $program build

        set kernel [opencl::createKernel $program "vector_add"]

        set buffer1 [opencl::createBuffer $context r double 1024]
        set buffer2 [opencl::createBuffer $context r double 1024]
        set buffer3 [opencl::createBuffer $context w double 1024]

        $queue enqueueWriteBuffer $buffer1 $A
        $queue enqueueWriteBuffer $buffer2 $B

        $kernel setKernelArg 0 mem $buffer1
        $kernel setKernelArg 1 mem $buffer2
        $kernel setKernelArg 2 mem $buffer3

        $queue enqueueNDRangeKernel $kernel 1024 64
        $queue finish

        set C [$queue enqueueReadBuffer $buffer3]
        $queue finish

        ::vectcl::vexpr {D = A + B}

        set work 1
        for {set i 0} {$i < 1024} {incr i} {
            set clvalue [numarray::get $C $i]
            set ckvalue [numarray::get $D $i]
            if {$clvalue != $ckvalue} {
                puts "Something is wrong..."
                set work 0
                break
            }
        }

        if {$work == 1} {
            puts "Done."
        }

        $buffer1 close
        $buffer2 close
        $buffer3 close
        $queue close
        $kernel close
        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }

Create a binary file -

    package require vectcl
    package require opencl

    set code {__kernel void vector_add(__global const double *A, __global const double *B, __global double *C) {
        int i = get_global_id(0);
        C[i] = A[i] + B[i];
    }
    }

    try {
        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]
        set context [opencl::createContext $device $platform]

        set program [opencl::createProgramWithSource $context $code]
        $program build
        puts "File size: [$program info binary_sizes]"
        set binary [$program info binaries]

        set infile [open "vector.bin" "w+b"]
        puts -nonewline $infile $binary
        close $infile

        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }

Execue a function by using binary file -

    package require vectcl
    package require opencl

    try {
        set A [numarray::constfill 5.0 1024]
        set B [numarray::constfill 10.0 1024]

        set myfile [open "vector.bin" "r+b"]
        set binary [read -nonewline $myfile]
        close $myfile

        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]
        set context [opencl::createContext $device $platform]
        set queue [opencl::createCommandQueue $context $device]
        set program [opencl::createProgramWithBinary $context $device $binary]
        $program build

        set kernel [opencl::createKernel $program "vector_add"]

        set buffer1 [opencl::createBuffer $context r double 1024]
        set buffer2 [opencl::createBuffer $context r double 1024]
        set buffer3 [opencl::createBuffer $context w double 1024]

        $queue enqueueWriteBuffer $buffer1 $A
        $queue enqueueWriteBuffer $buffer2 $B

        $kernel setKernelArg 0 mem $buffer1
        $kernel setKernelArg 1 mem $buffer2
        $kernel setKernelArg 2 mem $buffer3

        $queue enqueueNDRangeKernel $kernel 1024 64
        $queue finish
        set C [$queue enqueueReadBuffer $buffer3]
        $queue finish

        ::vectcl::vexpr {D = A + B}

        set work 1
        for {set i 0} {$i < 1024} {incr i} {
            set clvalue [numarray::get $C $i]
            set ckvalue [numarray::get $D $i]
            if {$clvalue != $ckvalue} {
                puts "Something is wrong..."
                set work 0
                break
            }
        }

        if {$work == 1} {
            puts "Done."
        }

        $buffer1 close
        $buffer2 close
        $buffer3 close
        $queue close
        $kernel close
        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }
