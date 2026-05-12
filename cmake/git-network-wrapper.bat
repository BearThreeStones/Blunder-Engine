@echo off
git -c http.version=HTTP/1.1 -c http.sslBackend=schannel -c http.schannelCheckRevoke=false -c url.https://github.com/.insteadOf=https://skia.googlesource.com/external/github.com/ -c url.https://github.com/.insteadOf=https://chromium.googlesource.com/external/github.com/ %*
