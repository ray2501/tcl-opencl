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
commandQueueHandle enqueueNDRangeKernel kernel work_dim global_work_size local_work_size  
commandQueueHandle flush  
commandQueueHandle finish  
commandQueueHandle close  

When work_dim > 1, global_work_size and local_work_size should be a list.


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
buffer, image, int, long, double


### Memory Object

::opencl::createBuffer context flags type length  
bufferHandle close  

`flags` value should be -  
r, w, rw  

`type` value should be -  
int, long, double

::opencl::createImage context flags format imagetype width height  
imageHandle close  

`flags` value should be -  
r, w, rw  

format value should be -  
a, rgba  

imagetype value should be -  
image2d  


UNIX BUILD
=====

Before building tcl-opencl, you need have VecTcl and ocl-icd development
files, and opencl-headers header files.

Building under most UNIX systems is easy, just run the configure script
and then run make. For more information about the build process, see
the tcl/unix/README file in the Tcl src dist. The following minimal
example will install the extension in the /opt/tcl directory.

    $ cd tcl-opencl
    $ ./configure --prefix=/opt/tcl
    $ make
    $ make install

If you need setup directory containing tcl configuration (tclConfig.sh),
below is an example:

    $ cd tcl-opencl
    $ ./configure --with-tcl=/opt/activetcl/lib
    $ make
    $ make install


Windows BUILD
=====

### MSYS2

[Msys2](https://www.msys2.org/)  provides a Unix-style build while generating
native Windows binaries. Using the Msys2 build tools means that you can use
the same configure script as per the Unix build to create a Makefile.

Before building tcl-opencl, you need install VecTcl and have VecTcl
development files.

Using below command to install x86_64 OpenCL packages for Mingw-w64
(not sync to the latest code, but still can work):

    pacman -S mingw-w64-x86_64-opencl-icd-git mingw-w64-x86_64-opencl-headers

If you need setup directory containing tcl and VecTcl configuration (tclConfig.sh
and vectclConfig.sh), below is an example:

    $ cd tcl-opencl
    $ ./configure --prefix=/opt/tcl --with-tcl=/opt/tcl/lib --with-vectcl=/opt/tcl/lib/vectcl0.3
    $ make
    $ make install

You also need to install an OpenCL implementation that can work in your
environment. For example, you can download
[CUDA Toolkit](https://developer.nvidia.com/cuda-downloads) for NVIDIA product.


Examples
=====

Get basic info -

    package require vectcl
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

        $kernel setKernelArg 0 buffer $buffer1

        for {set c 2} {$c <= $c_max} {incr c} {
            $kernel setKernelArg 1 int $c
            $queue enqueueNDRangeKernel $kernel 1 $N $N
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

        $kernel setKernelArg 0 buffer $buffer1
        $kernel setKernelArg 1 buffer $buffer2
        $kernel setKernelArg 2 buffer $buffer3

        $queue enqueueNDRangeKernel $kernel 1 1024 64
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

        $kernel setKernelArg 0 buffer $buffer1
        $kernel setKernelArg 1 buffer $buffer2
        $kernel setKernelArg 2 buffer $buffer3

        $queue enqueueNDRangeKernel $kernel 1 1024 64
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

Test image object function
(using [tcl-stbimage](https://github.com/ray2501/tcl-stbimage) to load image) -

    package require vectcl
    package require opencl
    package require stbimage

    set code {__kernel void PixelAccess(__read_only image2d_t imageIn,__write_only image2d_t imageOut)
    {
      sampler_t srcSampler = CLK_NORMALIZED_COORDS_FALSE | 
        CLK_ADDRESS_CLAMP_TO_EDGE |
        CLK_FILTER_NEAREST;
       int2 imageCoord = (int2) (get_global_id(0), get_global_id(1));
       uint4 pixel = read_imageui(imageIn, srcSampler, imageCoord);
       write_imageui (imageOut, imageCoord, pixel);
    }
    }

    if {$argc >= 1} {
        set filename [lindex $argv 0]
    } elseif {$argc == 0} {
        puts "Please input a filename"
        exit
    }

    try {
        set d [::stbimage::load $filename]
        set width [dict get $d width]
        set height [dict get $d height]
        set channels [dict get $d channels]

        if {$channels != 4} {
            puts "Not supported image format."
            exit
        }

        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]

        set isSupport [$device info image_support]
        if {$isSupport == 0 } {
            puts "Device doses not support images."
            exit
        } else {
            set max_height_2d [$device info image2d_max_height]
            set max_width_2d [$device info image2d_max_width]

            if {$max_height_2d <= $height || $max_width_2d <= $width} {
                puts "Error: height $height max $max_height_2d"
                puts "       width  $width max $max_width_2d"
                exit
            }
        }

        set context [opencl::createContext $device $platform]
        set queue [opencl::createCommandQueue $context $device]
        set program [opencl::createProgramWithSource $context $code]
        $program build

        set kernel [opencl::createKernel $program "PixelAccess"]

        set image1 [opencl::createImage $context r rgba image2d $width $height]
        set image2 [opencl::createImage $context rw rgba image2d $width $height]

        $queue enqueueWriteImage $image1 [dict get $d data]

        $kernel setKernelArg 0 image $image1
        $kernel setKernelArg 1 image $image2

        $queue enqueueNDRangeKernel $kernel 2 [list $width $height] [list 1 1]
        $queue finish

        set output [$queue enqueueReadImage $image2]
        $queue finish

        ::stbimage::write png output.png $width $height $channels $output

        $image1 close
        $image2 close
        $queue close
        $kernel close
        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }

Test image object function -

    package require vectcl
    package require opencl
    package require stbimage

    set code {__kernel void PixelAccess(__read_only image2d_t imageIn,__write_only image2d_t imageOut)
    {
      sampler_t srcSampler = CLK_NORMALIZED_COORDS_FALSE | 
        CLK_ADDRESS_CLAMP_TO_EDGE |
        CLK_FILTER_NEAREST;
       int2 imageCoord = (int2) (get_global_id(0), get_global_id(1));
       uint4 pixel = read_imageui(imageIn, srcSampler, imageCoord);
       write_imageui (imageOut, imageCoord, pixel);
    }
    }

    if {$argc >= 1} {
        set filename [lindex $argv 0]
    } elseif {$argc == 0} {
        puts "Please input a filename"
        exit
    }

    try {
        set d [::stbimage::load $filename]
        set width [dict get $d width]
        set height [dict get $d height]
        set channels [dict get $d channels]

        if {$channels != 1} {
            puts "Not supported image format."
            exit
        }

        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]

        set isSupport [$device info image_support]
        if {$isSupport == 0 } {
            puts "Device doses not support images."
            exit
        } else {
            set max_height_2d [$device info image2d_max_height]
            set max_width_2d [$device info image2d_max_width]

            if {$max_height_2d <= $height || $max_width_2d <= $width} {
                puts "Error: height $height max $max_height_2d"
                puts "       width  $width max $max_width_2d"
                exit
            }
        }

        set context [opencl::createContext $device $platform]
        set queue [opencl::createCommandQueue $context $device]
        set program [opencl::createProgramWithSource $context $code]
        $program build

        set kernel [opencl::createKernel $program "PixelAccess"]

        set image1 [opencl::createImage $context r a image2d $width $height]
        set image2 [opencl::createImage $context w a image2d $width $height]

        $queue enqueueWriteImage $image1 [dict get $d data]

        $kernel setKernelArg 0 image $image1
        $kernel setKernelArg 1 image $image2

        $queue enqueueNDRangeKernel $kernel 2 [list $width $height] [list 1 1]
        $queue finish

        set output [$queue enqueueReadImage $image2]
        $queue finish

        ::stbimage::write png output.png $width $height $channels $output

        $image1 close
        $image2 close
        $queue close
        $kernel close
        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }

Generate a grayscale image -

    package require vectcl
    package require opencl
    package require stbimage

    set code {__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                                    CLK_ADDRESS_CLAMP_TO_EDGE |
                                    CLK_FILTER_LINEAR;

    __kernel void Grayscale(__read_only image2d_t imgIn, __write_only image2d_t imgOut) {
        int2 gid = (int2)(get_global_id(0), get_global_id(1));
        int2 size = get_image_dim(imgIn);

        if(all(gid < size)){
            uint4 pixel = read_imageui(imgIn, sampler, gid);
            float4 color = convert_float4(pixel) / 255;
            color.xyz = 0.2126*color.x + 0.7152*color.y + 0.0722*color.z;
            pixel = convert_uint4_rte(color * 255);
            write_imageui(imgOut, gid, pixel);
        }
    }
    }

    if {$argc >= 1} {
        set filename [lindex $argv 0]
    } elseif {$argc == 0} {
        puts "Please input a filename"
        exit
    }

    try {
        set d [::stbimage::load $filename]
        set width [dict get $d width]
        set height [dict get $d height]
        set channels [dict get $d channels]

        if {$channels != 4} {
            puts "Not supported image format."
            exit
        }

        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]

        set isSupport [$device info image_support]
        if {$isSupport == 0 } {
            puts "Device doses not support images."
            exit
        } else {
            set max_height_2d [$device info image2d_max_height]
            set max_width_2d [$device info image2d_max_width]

            if {$max_height_2d <= $height || $max_width_2d <= $width} {
                puts "Error: height $height max $max_height_2d"
                puts "       width  $width max $max_width_2d"
                exit
            }
        }

        set context [opencl::createContext $device $platform]
        set queue [opencl::createCommandQueue $context $device]
        set program [opencl::createProgramWithSource $context $code]
        $program build

        set kernel [opencl::createKernel $program "Grayscale"]

        set image1 [opencl::createImage $context r rgba image2d $width $height]
        set image2 [opencl::createImage $context rw rgba image2d $width $height]

        $queue enqueueWriteImage $image1 [dict get $d data]

        $kernel setKernelArg 0 image $image1
        $kernel setKernelArg 1 image $image2

        $queue enqueueNDRangeKernel $kernel 2 [list $width $height] [list 1 1]
        $queue finish

        set output [$queue enqueueReadImage $image2]
        $queue finish

        ::stbimage::write png output.png $width $height $channels $output

        $image1 close
        $image2 close
        $queue close
        $kernel close
        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }

It is using [CRIMP](https://core.tcl.tk/akupries/crimp/home) to read a
ppm image, and using [tcl-imagebytes](https://github.com/ray2501/tcl-imagebytes),
Tk photo image and CRIMP to write a ppm image.

    package require Tk
    package require vectcl
    package require opencl
    package require crimp
    package require crimp::ppm
    package require crimp::tk
    package require imagebytes

    set code {__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                                    CLK_ADDRESS_CLAMP_TO_EDGE |
                                    CLK_FILTER_LINEAR;

    __kernel void Grayscale(__read_only image2d_t imgIn, __write_only image2d_t imgOut) {
        int2 gid = (int2)(get_global_id(0), get_global_id(1));
        int2 size = get_image_dim(imgIn);

        if(all(gid < size)){
            uint4 pixel = read_imageui(imgIn, sampler, gid);
            float4 color = convert_float4(pixel) / 255;
            color.xyz = 0.2126*color.x + 0.7152*color.y + 0.0722*color.z;
            pixel = convert_uint4_rte(color * 255);
            write_imageui(imgOut, gid, pixel);
        }
    }
    }

    if {$argc >= 1} {
        set filename [lindex $argv 0]
    } elseif {$argc == 0} {
        puts "Please input a filename"
        exit
    }

    try {
        # It is using CRIMP to read a ppm image file and convert to RGBA format
        set f [open $filename "r+b"]
        set imgdata [read $f]
        close $f
        set img [::crimp read ppm $imgdata]
        set img [::crimp convert 2hsv $img]
        set img [::crimp convert 2rgba $img]
        set width [::crimp width $img]
        set height [::crimp height $img]
        set channels [llength [::crimp channels $img]]
        set data [::crimp pixel $img]

        if {$channels != 4} {
            puts "Not supported image format."
            exit
        }

        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]

        set isSupport [$device info image_support]
        if {$isSupport == 0 } {
            puts "Device doses not support images."
            exit
        } else {
            set max_height_2d [$device info image2d_max_height]
            set max_width_2d [$device info image2d_max_width]

            if {$max_height_2d <= $height || $max_width_2d <= $width} {
                puts "Error: height $height max $max_height_2d"
                puts "       width  $width max $max_width_2d"
                exit
            }
        }

        set context [opencl::createContext $device $platform]
        set queue [opencl::createCommandQueue $context $device]
        set program [opencl::createProgramWithSource $context $code]
        $program build

        set kernel [opencl::createKernel $program "Grayscale"]

        set image1 [opencl::createImage $context r rgba image2d $width $height]
        set image2 [opencl::createImage $context rw rgba image2d $width $height]

        $queue enqueueWriteImage $image1 $data

        $kernel setKernelArg 0 image $image1
        $kernel setKernelArg 1 image $image2

        $queue enqueueNDRangeKernel $kernel 2 [list $width $height] [list 1 1]
        $queue finish

        set output [$queue enqueueReadImage $image2]
        $queue finish

        # I don't know how to write raw data to CRIMP image obj directly.
        # Below code is using imagebytes, Tk photo image and ::crimp::tk function.
        image create photo imgobj
        bytesToPhoto $output imgobj $width $height $channels
        set outputimg [::crimp read tk imgobj]
        ::crimp write 2file ppm-raw "output.ppm" $outputimg

        $image1 close
        $image2 close
        $queue close
        $kernel close
        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }

    exit

Below example is using Tk
via [tcl-imagebytes](https://github.com/ray2501/tcl-imagebytes) to
read and write a png file (Tk provides built-in support for PNG since 8.6) -

    package require Tk
    package require vectcl
    package require opencl
    package require imagebytes

    set code {__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                                    CLK_ADDRESS_CLAMP_TO_EDGE |
                                    CLK_FILTER_LINEAR;

    __kernel void Grayscale(__read_only image2d_t imgIn, __write_only image2d_t imgOut) {
        int2 gid = (int2)(get_global_id(0), get_global_id(1));
        int2 size = get_image_dim(imgIn);

        if(all(gid < size)){
            uint4 pixel = read_imageui(imgIn, sampler, gid);
            float4 color = convert_float4(pixel) / 255;
            color.xyz = 0.2126*color.x + 0.7152*color.y + 0.0722*color.z;
            pixel = convert_uint4_rte(color * 255);
            write_imageui(imgOut, gid, pixel);
        }
    }
    }

    if {$argc >= 1} {
        set filename [lindex $argv 0]
    } elseif {$argc == 0} {
        puts "Please input a filename"
        exit
    }

    try {
        image create photo imgobj -format png -file $filename
        set d [bytesFromPhoto imgobj]
        set width [dict get $d width]
        set height [dict get $d height]
        set channels [dict get $d channels]
        set data [dict get $d data]

        if {$channels != 4} {
            puts "Not supported image format."
            exit
        }

        set platform [opencl::getPlatformID 1]
        set device [opencl::getDeviceID $platform default 1]

        set isSupport [$device info image_support]
        if {$isSupport == 0 } {
            puts "Device doses not support images."
            exit
        } else {
            set max_height_2d [$device info image2d_max_height]
            set max_width_2d [$device info image2d_max_width]

            if {$max_height_2d <= $height || $max_width_2d <= $width} {
                puts "Error: height $height max $max_height_2d"
                puts "       width  $width max $max_width_2d"
                exit
            }
        }

        set context [opencl::createContext $device $platform]
        set queue [opencl::createCommandQueue $context $device]
        set program [opencl::createProgramWithSource $context $code]
        $program build

        set kernel [opencl::createKernel $program "Grayscale"]

        set image1 [opencl::createImage $context r rgba image2d $width $height]
        set image2 [opencl::createImage $context rw rgba image2d $width $height]

        $queue enqueueWriteImage $image1 $data

        $kernel setKernelArg 0 image $image1
        $kernel setKernelArg 1 image $image2

        $queue enqueueNDRangeKernel $kernel 2 [list $width $height] [list 1 1]
        $queue finish

        set output [$queue enqueueReadImage $image2]
        $queue finish

        bytesToPhoto $output imgobj $width $height $channels
        imgobj write "output.png" -format png

        $image1 close
        $image2 close
        $queue close
        $kernel close
        $program close
        $context close
        $device close
        $platform close
    } on error {em} {
        puts $em
    }

    exit
