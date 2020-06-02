%define _version %(echo "$(curl --silent "https://api.github.com/repos/coozoo/primus-vk/tags"|grep name|head -1|grep -oP '(?<=name\": \").+(?=\",)')")

%define _date #%(date +"%Y%m%d")

Name:           primus-vk
Version:        %(echo "$(curl --silent "https://api.github.com/repos/coozoo/primus-vk/tags"|grep name|head -1|grep -oP '(?<=name\": \").+(?=\",)')")
Release:        %{_date}%{?dist}
Summary:        Primus-Vk Nvidia Vulkan offloading for Bumblebee
License:        BSD
Group:          Hardware/Other
Url:            https://github.com/felixdoerre/primus_vk
Source0:        https://github.com/coozoo/primus-vk/archive/master.zip#/%{name}-%{version}.gz
Source1:        https://raw.githubusercontent.com/coozoo/primus-vk/master/rpm/pvkrun
Source2:        https://raw.githubusercontent.com/coozoo/primus-vk/master/rpm/primus_vk_wrapper.json

# Patch for makefile to use provided compiler flags
Patch0:         https://raw.githubusercontent.com/coozoo/primus-vk/master/Makefile.patch

BuildRequires: git
BuildRequires: gcc-c++
BuildRequires: vulkan-devel
BuildRequires: vulkan-headers
BuildRequires: wayland-devel
BuildRequires: libxcb-devel
BuildRequires: mesa-libGL-devel
%if 0%{?fedora} >= 29
BuildRequires: vulkan-validation-layers-devel
%endif

Requires:       vulkan-filesystem
Requires:       bumblebee
Requires:       %{name}-libs%{?_isa} = %{version}-%{release}

%description
This Vulkan layer can be used to do GPU offloading. Typically you want to display an image rendered on a more powerful GPU on a display managed by an internal GPU.
It is basically the same as Primus for OpenGL (https://github.com/amonakov/primus). However it does not wrap the Vulkan API from the application but is directly integrated into Vulkan as a layer (which seems to be the intendend way to implement such logic).

%package libs
Version:        %{version}
Release:        %{release}
Summary:        Primus-Vk libraries
License:        BSD

%description libs
%{summary}

%prep
%setup -n primus-vk-master
%patch0 -p0

%build
export CXXFLAGS="%{optflags}"
make %{?_smp_mflags}

%install
install -D "libnv_vulkan_wrapper.so" "%{buildroot}%{_libdir}/libnv_vulkan_wrapper.so"
install -D "libprimus_vk.so" "%{buildroot}%{_libdir}/libprimus_vk.so"
install -Dm 755 "primus_vk_diag" "%{buildroot}%{_bindir}/primus_vk_diag"
install -Dm 755 "%{_builddir}/primus-vk-master/rpm/pvkrun" "%{buildroot}%{_bindir}/pvkrun"
install -Dm 644 "primus_vk.json" "%{buildroot}%{_datadir}/vulkan/implicit_layer.d/primus_vk.json"
install -Dm 644 "%{_builddir}/primus-vk-master/rpm/primus_vk_wrapper.json" "%{buildroot}%{_datadir}/vulkan/icd.d/primus_vk_wrapper.json"

%post
ICDLIST=$(find /etc/vulkan/icd.d/ /usr/share/vulkan/icd.d/ -name "nvidia_icd*.json" -type f)
for ICDFILE in $ICDLIST; do
	mv $ICDFILE $ICDFILE.pvk
done

%postun
if [ $1 == 0 ]; then
	ICDPVKLIST=$(find /etc/vulkan/icd.d/ /usr/share/vulkan/icd.d/ -name "nvidia_icd*.json.pvk" -type f)
	for ICDFILEPVK in $ICDPVKLIST; do
		ICDFILE=$(echo $ICDFILEPVK | sed 's/.pvk//')
		mv $ICDFILEPVK $ICDFILE
	done
fi

%files
%defattr(-,root,root)
%doc LICENSE README.md
%{_bindir}/primus_vk_diag
%{_bindir}/pvkrun
%{_datadir}/vulkan/implicit_layer.d/primus_vk.json
%{_datadir}/vulkan/icd.d/primus_vk_wrapper.json

%files libs
%{_libdir}/libnv_vulkan_wrapper.so
%{_libdir}/libnv_vulkan_wrapper.so.1
%{_libdir}/libprimus_vk.so
%{_libdir}/libprimus_vk.so.1

%changelog
* Mon Jun 1 2020 Yuriy Kuzin
integrate some work of Leonid Maksymchuk <leonmaxx@4menteam.com>
and chage it to first build
