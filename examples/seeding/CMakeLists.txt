nvbio_module(seeding)

addsources(
seeding.cu
)

cuda_add_executable(seeding ${seeding_srcs})
target_link_libraries(seeding nvbio zlibstatic crcstatic ${SYSTEM_LINK_LIBRARIES})
