Name:    kdegraphics
Summary: KDE Graphics Applications
Epoch:   7
Version: 4.10.5
Release: 3%{?dist}

Group:   Applications/Multimedia
License: GPLv2
URL:     http://www.kde.org/
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch: noarch

BuildRequires:  kde-filesystem

Provides: kdegraphics4 = %{version}-%{release}

Requires: %{name}-libs = %{epoch}:%{version}-%{release}

Requires: gwenview >= %{version}
%if 0%{?fedora}
Requires: kamera >= %{version}
Requires: kdegraphics-mobipocket >= %{version}
Requires: ksaneplugin >= %{version}
%endif
Requires: kcolorchooser >= %{version}
Requires: kdegraphics-strigi-analyzer >= %{version}
Requires: kdegraphics-thumbnailers >= %{version}
Requires: kgamma >= %{version}
Requires: kolourpaint >= %{version}
Requires: kruler >= %{version}
Requires: ksnapshot >= %{version}
Requires: okular >= %{version}
Requires: svgpart >= %{version}

%description
Kdegraphics metapackage, to ease migration to split applications

%package common 
Summary: Common files for %{name}
%description common 
%{summary}.

%package libs
Summary: Runtime libraries for %{name}
Group:   System Environment/Libraries
Requires: libkipi >= %{version}
Requires: libkdcraw >= %{version}
Requires: libkexiv2 >= %{version}
Requires: libksane >= %{version}
%description libs
%{summary}.

%package devel
Group:    Development/Libraries
Summary:  Developer files for %{name}
Requires: %{name}-libs = %{epoch}:%{version}-%{release}
Requires: libkipi-devel
Requires: libkdcraw-devel
Requires: libkexiv2-devel
Requires: libksane-devel
Requires: okular-devel
%description devel
%{summary}.

%prep
# blank

%build
# blank


%install
# blank


%files
# blank

%files libs
# blank

%files devel
# blank


%changelog
* Fri Dec 27 2013 Daniel Mach <dmach@redhat.com> - 7:4.10.5-3
- Mass rebuild 2013-12-27

* Fri Jul 05 2013 Than Ngo <than@redhat.com> - 7:4.10.5-2
- cleanup

* Sun Jun 30 2013 Than Ngo <than@redhat.com> - 4.10.5-1
- 4.10.5

* Sat Jun 01 2013 Rex Dieter <rdieter@fedoraproject.org> - 7:4.10.4-1
- 4.10.4

* Mon May 06 2013 Than Ngo <than@redhat.com> - 4.10.3-1
- 4.10.3

* Sun Mar 31 2013 Rex Dieter <rdieter@fedoraproject.org> - 7:4.10.2-1
- 4.10.2

* Sat Mar 02 2013 Rex Dieter <rdieter@fedoraproject.org> - 7:4.10.1-1
- 4.10.1

* Fri Feb 01 2013 Rex Dieter <rdieter@fedoraproject.org> - 7:4.10.0-1
- 4.10.0

* Tue Jan 22 2013 Rex Dieter <rdieter@fedoraproject.org> - 7:4.9.98-1
- 4.9.98

* Fri Jan 04 2013 Rex Dieter <rdieter@fedoraproject.org> - 7:4.9.97-1
- 4.9.97

* Thu Dec 20 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.9.95-1
- 4.9.95

* Tue Dec 04 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.9.90-1
- 4.9.90

* Sat Nov 03 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.9.3-1
- 4.9.3

* Sat Sep 29 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.9.2-1
- 4.9.2

* Wed Sep 05 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.9.1-1
- 4.9.1

* Fri Jul 27 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.9.0-1
- 4.9.0

* Thu Jul 19 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 7:4.8.97-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Wed Jul 11 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.8.97-1
- 4.8.97

* Thu Jun 28 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.8.95-1
- 4.8.95

* Sat Jun 09 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.8.90-1
- 4.8.90

* Wed Jun 06 2012 Than Ngo <than@redhat.com> - 7:4.8.4-1
- 4.8.4

* Mon May 21 2012 Than Ngo <than@redhat.com> - 7:4.8.3-4
- rhel/fedora condition

* Sun May 20 2012 Rex Dieter <rdieter@fedoraproject.org> 7:4.8.3-3
- +Requires: kdegraphics-mobipocket

* Wed May 09 2012 Than Ngo <than@redhat.com> - 7:4.8.3-2
- add rhel/fedora condition

* Mon Apr 30 2012 Jaroslav Reznik <jreznik@redhat.com> - 7:4.8.3-1
- 4.8.3

* Fri Mar 30 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.8.2-1
- 4.8.2

* Mon Mar 05 2012 Jaroslav Reznik <jreznik@redhat.com> - 7:4.8.1-1
- 4.8.1

* Sun Jan 22 2012 Rex Dieter <rdieter@fedoraproject.org> - 7:4.8.0-1
- 4.8.0

* Wed Jan 04 2012 Rex Dieter <rdieter@fedoraproject.org> 7:4.7.97-1
- 4.7.97

* Thu Dec 22 2011 Radek Novacek <rnovacek@redhat.com> - 7:4.7.95-1
- 4.7.95

* Thu Dec 08 2011 Rex Dieter <rdieter@fedoraproject.org> 4.7.90-1
- 4.7.90

* Sat Dec 03 2011 Rex Dieter <rdieter@fedoraproject.org> 4.7.80-1
- 4.7.80

* Sat Dec 03 2011 Rex Dieter <rdieter@fedoraproject.org> 4.7.4-1
- 4.7.4

* Sat Oct 29 2011 Rex Dieter <rdieter@fedoraproject.org> 4.7.3-1
- 4.7.3

* Wed Oct 05 2011 Rex Dieter <rdieter@fedoraproject.org> 4.7.2-1
- 4.7.2

* Mon Sep 19 2011 Marek Kasik <mkasik@redhat.com> - 7:4.7.1-2
- Rebuild (poppler-0.17.3)

* Thu Sep 15 2011 Radek Novacek <rnovacek@redhat.com> 7:4.7.1-1
- 4.7.1

* Fri Aug 05 2011 Rex Dieter <rdieter@fedoraproject.org> 4.7.0-1
- 4.7.0

* Fri Jul 15 2011 Marek Kasik <mkasik@redhat.com> - 7:4.6.95-11
- Rebuild (poppler-0.17.0)

* Mon Jul 15 2011 Rex Dieter <rdieter@fedoraproject.org> 7:4.6.95-10
- placeholder metapackage

-* Mon Jul 11 2011 Jaroslav Reznik <jreznik@redhat.com> - 7:4.6.95-1
- 4.6.95 (rc2)

* Mon Jun 27 2011 Than Ngo <than@redhat.com> - 7:4.6.90-1
- 4.6.90 (rc1)

* Thu Jun 23 2011 Radek Novacek <rnovacek@redhat.com> - 4.6.80-1
- update to 4.6.80
- built from multiple tarballs
- add BR: kdebase-devel (libkonq) and kipi-plugins

* Mon Jun 06 2011 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.6.3-2
- fix printing of landscape documents in Okular (#509645, kde#181290)

* Thu Apr 28 2011 Rex Dieter <rdieter@fedoraproject.org> 4.6.3-1
- 4.6.3

* Wed Apr 06 2011 Than Ngo <than@redhat.com> - 7:4.6.2-1
- 4.6.2

* Sat Mar 19 2011 Dan Horák <dan[at]danny.cz> 4.6.1-2
- drop the s390(x) specifics

* Sun Feb 27 2011 Rex Dieter <rdieter@fedoraproject.org> 4.6.1-1
- 4.6.1

* Mon Feb 07 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 7:4.6.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Fri Jan 21 2011 Jaroslav Reznik <jreznik@redhat.com> - 7:4.6.0-1
- 4.6.0
- hardcode libjpeg version to 62

* Wed Jan 05 2011 Jaroslav Reznik <jreznik@redhat.com> - 7:4.5.95-1
- 4.5.95 (4.6rc2)

* Sat Jan 01 2011 Rex Dieter <rdieter@fedoraproject.org> - 7:4.5.90-2
- rebuild (exiv2,poppler)

* Wed Dec 22 2010 Rex Dieter <rdieter@fedoraproject.org> 4.5.90-1
- 4.5.90 (4.6rc1)

* Sat Dec 04 2010 Thomas Janssen <thomasj@fedoraproject.org> 4.5.85-1
- 4.5.85 (4.6beta2)

* Mon Nov 22 2010 Kevin Kofler <Kevin@tigcc.ticalc.org> - 4.5.80-2
- don't hardcode paths in OkularConfig.cmake

* Sat Nov 20 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.5.80-1
- 4.5.80 (4.6beta1)

* Sun Nov 07 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.5.3-2
- -libs: Provides libk*foo*

* Sun Oct 31 2010 Than Ngo <than@redhat.com> - 4.5.3-1
- 4.5.3

* Wed Oct 19 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.5.2-4
- fix typo in xps patch (#643833)

* Tue Oct 19 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.5.2-3
- okular fails to load XPS files (#643833)

* Tue Oct 05 2010 Lukas Tinkl <ltinkl@redhat.com> - 4.5.2-2
- tarball respin

* Fri Oct 01 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.5.2-1
- 4.5.2

* Tue Sep 07 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.5.1-2
- okular.desktop : Categories=-Office,+VectorGraphics (#591089)

* Fri Aug 27 2010 Jaroslav Reznik <jreznik@redhat.com> - 4.5.1-1
- 4.5.1

* Wed Aug 25 2010 Than Ngo <than@redhat.com> - 4.5.0-2
- Security fix, Okular PDB Processing Memory Corruption Vulnerability
  cve-2010-2575

* Tue Aug 03 2010 Than Ngo <than@redhat.com> - 4.5.0-1
- 4.5.0

* Sat Jul 25 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.4.95-1
- 4.5 RC3 (4.4.95)

* Wed Jul 07 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.4.92-1
- 4.5 RC2 (4.4.92)

* Sun Jul 04 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.4.90-2
- Missing kdebase-runtime dependency for Okular (kdegraphics) (#611118)

* Fri Jun 25 2010 Jaroslav Reznik <jreznik@redhat.com> - 4.4.90-1
- 4.5 RC1 (4.4.90)

* Tue Jun 22 2010 Karsten Hopp <karsten@redhat.com> 4.4.85-3
- fix ksane provides on s390 
- drop sane related part of make check on s390

* Tue Jun 08 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.4.85-2
- update libkdcraw-devel, libkexiv2-devel, libkipi-devel, libksane-devel deps

* Mon Jun 07 2010 Jaroslav Reznik <jreznik@redhat.com> - 4.4.85-1
- 4.5 Beta 2 (4.4.85)

* Mon May 31 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.4.80-2
- rebuild (exiv2)

* Fri May 21 2010 Jaroslav Reznik <jreznik@redhat.com> - 4.4.80-1
- 4.5 Beta 1 (4.4.80)

* Fri Apr 30 2010 Jaroslav Reznik <jreznik@redhat.com> - 4.4.3-1
- 4.4.3

* Thu Apr 01 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.4.2-3
- base qt47 patch on upstream fix in trunk/ instead

* Mon Mar 29 2010 Lukas Tinkl <ltinkl@redhat.com> - 4.4.2-1
- 4.4.2

* Sat Feb 27 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.4.1-1
- 4.4.1

* Fri Feb 05 2010 Than Ngo <than@redhat.com> - 4.4.0-1
- 4.4.0

* Sun Jan 31 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.3.98-1
- KDE 4.3.98 (4.4rc3)

* Wed Jan 20 2010 Lukas Tinkl <ltinkl@redhat.com> - 4.3.95-1
- KDE 4.3.95 (4.4rc2)

* Wed Jan 06 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.3.90-1
- kde-4.3.90 (4.4rc1)

* Mon Jan 03 2010 Rex Dieter <rdieter@fedoraproject.org> - 4.3.85-2
- rebuild (exiv2)

* Fri Dec 18 2009 Rex Dieter <rdieter@fedoraproject.org> - 4.3.85-1
- kde-4.3.85 (4.4beta2)

* Wed Dec 16 2009 Jaroslav Reznik <jreznik@redhat.com> - 4.3.80-2
- Repositioning the KDE Brand (#547361)

* Tue Dec  1 2009 Lukáš Tinkl <ltinkl@redhat.com> - 4.3.80-1
- KDE 4.4 beta1 (4.3.80)

* Tue Nov 24 2009 Kevin Kofler <Kevin@tigcc.ticalc.org> - 4.3.75-0.2.svn1048496
- update libkdcraw, libkexiv2, libkipi Provides versions

* Sun Nov 22 2009 Ben Boeckel <MathStuf@gmail.com> - 4.3.75-0.1.svn1048496
- update to 4.3.75 snapshot

* Sat Oct 31 2009 Rex Dieter <rdieter@fedoraproject.org> - 4.3.3-1
- 4.3.3

* Thu Oct 08 2009 Rex Dieter <rdieter@fedoraproject.org> - 4.3.2-3
- okular does not handle escaped URL correctly (kde#207461)

* Thu Oct 08 2009 Than Ngo <than@redhat.com> - 4.3.2-2
- rhel cleanup

* Mon Oct 05 2009 Than Ngo <than@redhat.com> - 4.3.2-1
- 4.3.2

* Fri Aug 28 2009 Than Ngo <than@redhat.com> - 4.3.1-1
- 4.3.1

* Thu Jul 30 2009 Than Ngo <than@redhat.com> - 4.3.0-1
- 4.3.0

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 7:4.2.98-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Wed Jul 22 2009 Than Ngo <than@redhat.com> - 4.2.98-1
- 4.3rc3

* Fri Jul 10 2009 Than Ngo <than@redhat.com> - 4.2.96-1
- 4.3rc2

* Fri Jun 26 2009 Than Ngo <than@redhat.com> - 4.2.95-1
- 4.3rc1

* Mon Jun 22 2009 Rex Dieter <rdieter@fedoraproject.org> - 4.2.90-2
- rebuild (poppler reduced libs)

* Wed Jun 03 2009 Rex Dieter <rdieter@fedoraproject.org> - 4.2.90-1
- KDE-4.3 beta2 (4.2.90)

* Wed May 27 2009 Rex Dieter <rdieter@fedoraproject.org> - 4.2.85-3
- fix non-gphoto/sane build, for s390 (#502827)
- drop < F-10 conditionals

* Wed May 20 2009 Kevin Kofler <Kevin@tigcc.ticalc.org> - 4.2.85-2
- rebuild for new Poppler

* Wed May 13 2009 Lukáš Tinkl <ltinkl@redhat.com> - 4.2.85-1
- KDE 4.3 beta 1

* Mon Apr 27 2009 Rex Dieter <rdieter@fedoraproject.org> - 4.2.2-5
- kio_msits subpkg, help avoid kchmviewer conflicts (#484861)

* Wed Apr 22 2009 Than Ngo <than@redhat.com> - 4.2.2-4
- fix build issue on s390(x)

* Fri Apr 03 2009 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.2.2-3
- work around Kolourpaint crash with Qt 4.5 (kde#183850)

* Wed Apr 01 2009 Rex Dieter <rdieter@fedoraproject.org> 4.2.2-2
- optimize scriptlets

* Tue Mar 31 2009 Lukáš Tinkl <ltinkl@redhat.com> - 4.2.2-1
- KDE 4.2.2

* Mon Mar 09 2009 Rex Dieter <rdieter@fedoraproject.org> 4.2.1-3
- gwenview-fix-version.diff

* Sun Mar 08 2009 Rex Dieter <rdieter@fedoraproject.org> 4.2.1-2
- missing dependency on kipiplugin.desktop (#489218)

* Fri Feb 27 2009 Than Ngo <than@redhat.com> - 4.2.1-1
- 4.2.1

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 7:4.2.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Sat Jan 31 2009 Rex Dieter <rdieter@fedoraproject.org> - 4.2.0-2
- unowned dirs (#483317)

* Thu Jan 22 2009 Than Ngo <than@redhat.com> - 4.2.0-1
- 4.2.0

* Sat Jan 17 2009 Rakesh Pandit <rakesh@fedoraproject.org> - 4.1.96-2
- Updated with new djvulibre

* Wed Jan 07 2009 Than Ngo <than@redhat.com> - 4.1.96-1
- 4.2rc1

* Mon Dec 22 2008 Rex Dieter <rdieter@fedoraproject.org> - 4.1.85-4
- -devel: Provides: libkipi-devel = 0.3.0

* Thu Dec 18 2008 Rex Dieter <rdieter@fedoraproject.org> - 4.1.85-3 
- respin (eviv2)

* Mon Dec 15 2008 Rex Dieter <rdieter@fedoraproject.org> 4.1.85-2
- BR: ebook-tools-devel

* Fri Dec 12 2008 Than Ngo <than@redhat.com> 4.1.85-1
- 4.2beta2
- BR: soprano-devel

* Mon Dec 01 2008 Rex Dieter <rdieter@fedoraproject.org> 4.1.80-3
- Obsoletes: libkdcraw libkexiv2 libkipi (F10+)
- cleanup Obsoletes: kdegraphics-extras

* Thu Nov 20 2008 Than Ngo <than@redhat.com> 4.1.80-2
- merged

* Thu Nov 20 2008 Lorenzo Villani <lvillani@binaryhelix.net> - 7:4.1.72-1
- 4.1.80
- BR cmake >= 2.6.2
- make install/fast

* Wed Nov 12 2008 Than Ngo <than@redhat.com> 4.1.3-1
- 4.1.3

* Wed Oct 29 2008 Rex Dieter <rdieter@fedoraproject.org> 4.1.2-4
- respin libkexiv2/libkdcraw backport patches

* Mon Oct 06 2008 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.1.2-3
- respun tarball
- backport latest libkexiv2 and libkdcraw from trunk

* Mon Sep 29 2008 Rex Dieter <rdieter@fedoraproject.org> 4.1.2-2
- make VERBOSE=1
- respin against new(er) kde-filesystem

* Fri Sep 26 2008 Rex Dieter <rdieter@fedoraproject.org> 4.1.2-1
- 4.1.2

* Fri Aug 29 2008 Than Ngo <than@redhat.com> 4.1.1-1
- 4.1.1

* Thu Aug 21 2008 Rex Dieter <rdieter@fedoraproject.org> 4.1.0-6
- f10+: Obsoletes/Provides: libkdcraw-devel, libkexiv2-devel, libkipi-devel

* Wed Aug 20 2008 Rex Dieter <rdieter@fedoraproject.org> 4.1.0-5
- fix "last page is not printed" (kde #160860)

* Tue Aug 12 2008 Than Ngo <than@redhat.com> 4.1.0-4
- fix crash in printing review in okular
- update all the configuration each time a document is open in okular

* Tue Jul 29 2008 Than Ngo <than@redhat.com> 4.1.0-3
- respun

* Fri Jul 25 2008 Than Ngo <than@redhat.com> 4.1.0-2
- respun

* Wed Jul 23 2008 Than Ngo <than@redhat.com> 4.1.0-1
- 4.1.0

* Mon Jul 21 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.99-2
- omit conflicting lib{kexiv2,kdcraw,kipi}-devel bits in F-9 builds (#452392)

* Fri Jul 18 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.99-1
- 4.0.99

* Fri Jul 11 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.98-1
- 4.0.98

* Sun Jul 06 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.85-1
- 4.0.85

* Fri Jun 27 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.84-1
- 4.0.84

* Wed Jun 25 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.83-2
- respin for exiv2

* Thu Jun 19 2008 Than Ngo <than@redhat.com> 4.0.83-1
- 4.0.83 (beta2)

* Sun Jun 15 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.82-1
- 4.0.82

* Mon May 26 2008 Than Ngo <than@redhat.com> 4.0.80-1
- 4.1 beta1

* Sat May 10 2008 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.0.72-2
- add BR qca2-devel (for encrypted ODF documents in Okular)

* Sat May 10 2008 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.0.72-1
- update to 4.0.72
- drop backported system-libspectre patch

* Thu Apr 03 2008 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.0.3-3
- rebuild (again) for the fixed %%{_kde4_buildtype}

* Mon Mar 31 2008 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.0.3-2
- rebuild for NDEBUG and _kde4_libexecdir

* Fri Mar 28 2008 Than Ngo <than@redhat.com> 4.0.3-1
- 4.0.3
- drop kdegraphics-4.0.2-poppler07.patch, it's included in 4.0.3

* Thu Mar 20 2008 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.0.2-4
- backport patch to support poppler 0.7 from KDE 4.0.3

* Wed Mar 19 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.2-3
- respin (poppler)

* Sat Mar 01 2008 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.0.2-2
- package new FindOkular.cmake (in -devel)

* Thu Feb 28 2008 Than Ngo <than@redhat.com> 4.0.2-1
- 4.0.2

* Fri Feb 01 2008 Kevin Kofler <Kevin@tigcc.ticalc.org> 4.0.1-2
- build against system libspectre (backported from KDE 4.1)

* Thu Jan 31 2008 Rex Dieter <rdieter@fedoraproject.org> 4.0.1-1
- kde-4.0.1

* Tue Jan 08 2008 Rex Dieter <rdieter[AT]fedoraproject.org> 4.0.0-1
- kde-4.0.0

* Fri Dec 14 2007 Rex Dieter <rdieter[AT]fedoraproject.org> 3.97.0-7
- License: GPLv2
- Obsoletes: -extras(-libs)
- cleanup BR's, scriptlets
- omit devel symlink hacks

* Tue Dec 11 2007 Kevin Kofler <Kevin@tigcc.ticalc.org> 3.97.0-4
- rebuild for changed _kde4_includedir

* Fri Dec 07 2007 Than Ngo <than@redhat.com> 3.97.0-3
- get rid of useless define for F9

* Thu Dec 06 2007 Kevin Kofler <Kevin@tigcc.ticalc.org> 3.97.0-2
- don't hardcode %%fedora
- Requires: lpr (provided by cups) for printing in Okular

* Thu Dec 06 2007 Than Ngo <than@redhat.com> 3.97.0-1
- 3.97.0

* Fri Nov 30 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.2-1
- kde-3.96.2

* Wed Nov 21 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.1-1
- kde-3.96.1
- also use epoch in changelog (also backwards)

* Wed Nov 21 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-9
- libs subpkg

* Wed Nov 21 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-8
- %%description updated
- sorted %%BuildRequires
- sorted  %%files

* Mon Nov 19 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-7
- BR: kde-filesystem >= 4
- License is GPLv2+

* Mon Nov 19 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-6
- re-work the "%%if's"

* Mon Nov 19 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-5
- BR: libXcomposite-devel
- BR: libXdamage-devel
- BR: libxkbfile-devel
- BR: libXv-devel
- BR: libXxf86misc-devel
- BR: libXScrnSaver-devel

* Sun Nov 18 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-4
- explicit require on kdebase-runtime (for icons)
- fix copy&paste errors in devel package

* Sat Nov 17 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-3
- name kdegraphics4 on fedora <= 9
- remove all but okular on fedora <= 9
- +BR: kde4-macros(api)
- remove unneeded require for kdepimlibs
- add defattr to devel package

* Thu Nov 15 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-2
- re-added epoch (from kdegraphics3)
- move libspectreOkular.so from devel to normal package

* Thu Nov 15 2007 Sebastian Vahl <fedora@deadbabylon.de> 7:3.96.0-1
- Initial version for Fedora
