These files were used in the Android FL Core setup to do a custom build of openssl that removed the version from the shared library name. This enabled the .net SSL code to be able to load the library on Android, and for certs to be imported from a file that was specified in the SSL_CERT_FILE environment variable.

openssl 3.x should support this type of build OOTB, but binary size was much larger. 

TBD if we care enough about that to use this custom build or not.