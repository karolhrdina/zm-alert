#!/usr/bin/env bash

################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  READ THE ZPROJECT/README.MD FOR INFORMATION ABOUT MAKING PERMANENT CHANGES. #
################################################################################

set -x
set -e

if [ "$BUILD_TYPE" == "default" ]; then
    # Tell travis to deploy all files in dist
    mkdir dist
    export ZM_ALERT_DEPLOYMENT=dist/*
    # Move archives to dist
    mv *.tar.gz dist
    mv *.zip dist
    # Generate hash sums
    cd dist
    md5sum *.zip *.tar.gz > MD5SUMS
    sha1sum *.zip *.tar.gz > SHA1SUMS
    cd -
elif [ "$BUILD_TYPE" == "bindings" ] && [ "$BINDING" == "jni" ]; then
    ( cd bindings/jni && TERM=dumb PKG_CONFIG_PATH=/tmp/lib/pkgconfig ./gradlew clean bintrayUpload )
    cp bindings/jni/android/zm_alert-android.jar zm_alert-android-0.1.0.jar
    export ZM_ALERT_DEPLOYMENT=zm_alert-android-0.1.0.jar
else
    export ZM_ALERT_DEPLOYMENT=""
fi
