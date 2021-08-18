%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}
%define packckname opencl

Name:          tcl-opencl
Summary:       Tcl extension for OpenCL
Version:       0.6
Release:       0
License:       MIT
Group:         Development/Libraries/Tcl
Source:        %{name}-%{version}.tar.gz
URL:           https://sites.google.com/site/ray2501/tcl-opencl 
BuildRequires: autoconf
BuildRequires: make
BuildRequires: tcl-devel >= 8.6
BuildRequires: vectcl-devel
BuildRequires: ocl-icd-devel
BuildRequires: opencl-headers
Requires:      tcl >= 8.6
Requires:      vectcl
Requires:      ocl-icd-devel
BuildRoot:     %{buildroot}

%description
It is a Tcl extension for OpenCL, and a VecTcl extension.

%prep
%setup -q -n %{name}-%{version}

%build
CFLAGS="%optflags" ./configure \
	--prefix=%{directory} \
	--exec-prefix=%{directory} \
	--libdir=%{directory}/%{_lib}
make 

%install
make DESTDIR=%{buildroot} pkglibdir=%{tcl_archdir}/%{packckname}%{version} install

%clean
rm -rf %buildroot

%files
%defattr(-,root,root)
%doc README.md LICENSE
%{tcl_archdir}
