define PKG_PATH_CONFIG
prefix=$(PREFIX)
libdir=$${prefix}/lib
includedir=$${prefix}/include
endef

define PKG_DESCRIBE_CONFIG
Name: libroce
Description: The RDMA over Converged Ethernet (RoCE) userspace library
Version: ${VERSION}
Cflags: -I$${includedir}
Libs: -L$${libdir} $(LIBS) -lroce
endef
