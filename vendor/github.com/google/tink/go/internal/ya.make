GO_LIBRARY()

LICENSE(Apache-2.0)

SRCS(internal.go)

END()

RECURSE(
    aead
    internalregistry
    monitoringutil
)
