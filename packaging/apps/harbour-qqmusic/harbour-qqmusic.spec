%global debug_package %{nil}
%define __os_install_post %{nil}
%{!?qqmusic_version:%{error:qqmusic_version macro must be defined}}
%{!?qqmusic_release:%{error:qqmusic_release macro must be defined}}
%{!?qqmusic_name:%{error:qqmusic_name macro must be defined}}

Name:           %{qqmusic_name}
Version:        %{qqmusic_version}
Release:        %{qqmusic_release}
Summary:        Native ZZ Music client for QQ Music on Sailfish OS
License:        MIT
Group:          Applications/Multimedia
BuildArch:      aarch64
AutoReqProv:    no
Requires:       libsailfishapp >= 1.2.17
Requires:       sailfishsilica-qt5 >= 1.2.154
Requires:       qt5-qtmultimedia >= 5.6.2
Requires:       qt5-qtmultimedia-plugin-mediaservice-gstmediaplayer
Requires:       qt5-qtmultimedia-plugin-audio-pulseaudio
Requires:       qt5-qtmultimedia-plugin-resourcepolicy-resourceqt
Requires:       libsailfishsecretsplugin
Requires:       sailfishsecretsdaemon
Requires:       sailfishsecretsdaemon-secretsplugins-default
Requires:       nemo-qml-plugin-notifications-qt5
Obsoletes:      listen1-sailfish < 2.34.0
Provides:       listen1-sailfish = 2.34.0
Obsoletes:      harbour-qqmusic
Provides:       harbour-qqmusic = %{version}-%{release}

%description
A touch-first, QQ-only native Qt 5 Sailfish Silica music client.

%prep

%install
rm -rf %{buildroot}
install -D -m0755 %{stagingdir}/harbour-qqmusic \
    %{buildroot}/usr/bin/harbour-qqmusic
install -d -m0755 %{buildroot}/usr/share/harbour-qqmusic
cp -a %{stagingdir}/qml %{buildroot}/usr/share/harbour-qqmusic/qml
install -D -m0644 %{stagingdir}/harbour-qqmusic.desktop \
    %{buildroot}/usr/share/applications/harbour-qqmusic.desktop
install -D -m0644 %{stagingdir}/harbour-qqmusic-zh_CN.qm \
    %{buildroot}/usr/share/translations/harbour-qqmusic-zh_CN.qm
for size in 86 108 128 172; do
    install -D -m0644 %{stagingdir}/icons/${size}x${size}/harbour-qqmusic.png \
        %{buildroot}/usr/share/icons/hicolor/${size}x${size}/apps/harbour-qqmusic.png
done
install -d -m0755 %{buildroot}/usr/share/licenses/%{name}
for notice in LICENSE LICENSE.listen1 QT-LICENSES.md UPSTREAM.md COMMUNITY-NOTICE.md; do
    install -m0644 %{stagingdir}/licenses/${notice} \
        %{buildroot}/usr/share/licenses/%{name}/${notice}
done

%files
%defattr(-,root,root,-)
/usr/bin/harbour-qqmusic
/usr/share/applications/harbour-qqmusic.desktop
/usr/share/harbour-qqmusic
/usr/share/icons/hicolor/86x86/apps/harbour-qqmusic.png
/usr/share/icons/hicolor/108x108/apps/harbour-qqmusic.png
/usr/share/icons/hicolor/128x128/apps/harbour-qqmusic.png
/usr/share/icons/hicolor/172x172/apps/harbour-qqmusic.png
%dir /usr/share/licenses/%{name}
%license /usr/share/licenses/%{name}/LICENSE
%license /usr/share/licenses/%{name}/LICENSE.listen1
%license /usr/share/licenses/%{name}/QT-LICENSES.md
%license /usr/share/licenses/%{name}/UPSTREAM.md
%license /usr/share/licenses/%{name}/COMMUNITY-NOTICE.md
/usr/share/translations/harbour-qqmusic-zh_CN.qm

%changelog
* Mon Aug 24 2026 Bonito Port Team <noreply@localhost> - 1.0.0-4.bonito.dev
- Repair native Qt 5 QR login over bounded MQTT-over-TLS WebSocket transport.
