/*
 * include/linux/ion.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2012, 2014 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_ION_H
#define _LINUX_ION_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct ion_handle;
/**
 * enum ion_heap_types - list of all possible types of heaps
 * @ION_HEAP_TYPE_SYSTEM:	 memory allocated via vmalloc
 * @ION_HEAP_TYPE_SYSTEM_CONTIG: memory allocated via kmalloc
 * @ION_HEAP_TYPE_CARVEOUT:	 memory allocated from a prereserved
 * 				 carveout heap, allocations are physically
 * 				 contiguous
 * @ION_HEAP_TYPE_IOMMU: IOMMU memory
 * @ION_HEAP_TYPE_CP:	 memory allocated from a prereserved
 *				carveout heap, allocations are physically
 *				contiguous. Used for content protection.
 * @ION_HEAP_TYPE_DMA:          memory allocated via DMA API
 * @ION_HEAP_END:		helper for iterating over heaps
 */
enum ion_heap_type {
	ION_HEAP_TYPE_SYSTEM,
	ION_HEAP_TYPE_SYSTEM_CONTIG,
	ION_HEAP_TYPE_CARVEOUT,
	ION_HEAP_TYPE_DMA,
	ION_HEAP_TYPE_CUSTOM, /* must be last so device specific heaps always
				 are at the end of this enum */
	ION_NUM_HEAPS = 16,
};

#define ION_HEAP_SYSTEM_MASK		(1 << ION_HEAP_TYPE_SYSTEM)
#define ION_HEAP_SYSTEM_CONTIG_MASK	(1 << ION_HEAP_TYPE_SYSTEM_CONTIG)
#define ION_HEAP_CARVEOUT_MASK		(1 << ION_HEAP_TYPE_CARVEOUT)
#define ION_HEAP_TYPE_DMA_MASK         (1 << ION_HEAP_TYPE_DMA)

/**
 * heap flags - the lower 16 bits are used by core ion, the upper 16
 * bits are reserved for use by the heaps themselves.
 */
#define ION_FLAG_CACHED 1		/* mappings of this buffer should be
					   cached, ion will do cache
					   maintenance when the buffer is
					   mapped for dma */

#ifdef __KERNEL__
#include <linux/err.h>
#include <mach/ion.h>
struct ion_device;
struct ion_heap;
struct ion_mapper;
struct ion_client;
struct ion_buffer;

/* This should be removed some day when phys_addr_t's are fully
   plumbed in the kernel, and all instances of ion_phys_addr_t should
   be converted to phys_addr_t.  For the time being many kernel interfaces
   do not accept phys_addr_t's that would have to */
#define ion_phys_addr_t unsigned long
#define ion_virt_addr_t unsigned long

/**
 * struct ion_platform_heap - defines a heap in the given platform
 * @type:	type of the heap from ion_heap_type enum
 * @id:		unique identifier for heap.  When allocating (lower numbers
 * 		will be allocated from first)
 * @name:	used for debug purposes
 * @base:	base address of heap in physical memory if applicable
 * @size:	size of the heap in bytes if applicable
 * @memory_type:Memory type used for the heap
 * @has_outer_cache:    set to 1 if outer cache is used, 0 otherwise.
 * @extra_data:	Extra data specific to each heap type
 * @priv:	heap private data
 */
struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	ion_phys_addr_t base;
	size_t size;
	enum ion_memory_types memory_type;
	unsigned int has_outer_cache;
	void *extra_data;
	void *priv;
};

/**
 * struct ion_platform_data - array of platform heaps passed from board file
 * @has_outer_cache:    set to 1 if outer cache is used, 0 otherwise.
 * @nr:    number of structures in the array
 * @request_region: function to be called when the number of allocations goes
 *						from 0 -> 1
 * @release_region: function to be called when the number of allocations goes
 *						from 1 -> 0
 * @setup_region:   function to be called upon ion registration
 * @heaps: array of platform_heap structions
 *
 * Provided by the board file in the form of platform data to a platform device.
 */
struct ion_platform_data {
	unsigned int has_outer_cache;
	int nr;
	int (*request_region)(void *);
	int (*release_region)(void *);
	void *(*setup_region)(void);
	struct ion_platform_heap *heaps;
};

#ifdef CONFIG_ION

/**
 * ion_reserve() - reserve memory for ion heaps if applicable
 * @data:	platform data specifying starting physical address and
 *		size
 *
 * Calls memblock reserve to set aside memory for heaps that are
 * located at specific memory addresses or of specfic sizes not
 * managed by the kernel
 */
void ion_reserve(struct ion_platform_data *data);

/**
 * ion_client_create() -  allocate a client and returns it
 * @dev:	the global ion device
 * @heap_mask:	mask of heaps this client can allocate from
 * @name:	used for debugging
 */
struct ion_client *ion_client_create(struct ion_device *dev,
				     unsigned int heap_mask, const char *name);

/**
 *  msm_ion_client_create - allocate a client using the ion_device specified in
 *				drivers/gpu/ion/msm/msm_ion.c
 *
 * heap_mask and name are the same as ion_client_create, return values
 * are the same as ion_client_create.
 */

struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name);

/**
 * ion_client_destroy() -  free's a client and all it's handles
 * @client:	the client
 *
 * Free the provided client and all it's resources including
 * any handles it is holding.
 */
void ion_client_destroy(struct ion_client *client);

/**
 * ion_alloc - allocate ion memory
 * @client:	the client
 * @len:	size of the allocation
 * @align:	requested allocation alignment, lots of hardware blocks have
 *		alignment requirements of some kind
 * @heap_mask:	mask of heaps to allocate from, if multiple bits are set
 *		heaps will be tried in order from lowest to highest order bit
 * @flags:	heap flags, the low 16 bits are consumed by ion, the high 16
 *		bits are passed on to the respective heap and can be heap
 *		custom
 *
 * Allocate memory in one of the heaps provided in heap mask and return
 * an opaque handle to it.
 */
struct ion_handle *ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int heap_mask,
			     unsigned int flags);

/**
 * ion_free - free a handle
 * @client:	the client
 * @handle:	the handle to free
 *
 * Free the provided handle.
 */
void ion_free(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_phys - returns the physical address and len of a handle
 * @client:	the client
 * @handle:	the handle
 * @addr:	a pointer to put the address in
 * @len:	a pointer to put the length in
 *
 * This function queries the heap for a particular handle to get the
 * handle's physical address.  It't output is only correct if
 * a heap returns physically contiguous memory -- in other cases
 * this api should not be implemented -- ion_sg_table should be used
 * instead.  Returns -EINVAL if the handle is invalid.  This has
 * no implications on the reference counting of the handle --
 * the returned value may not be valid if the caller is not
 * holding a reference.
 */
int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len);

/**
 * ion_map_dma - return an sg_table describing a handle
 * @client:	the client
 * @handle:	the handle
 *
 * This function returns the sg_table describing
 * a particular ion handle.
 */
struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle);

/**
 * ion_map_kernel - create mapping for the given handle
 * @client:	the client
 * @handle:	handle to map
 * @flags:	flags for this mapping
 *
 * Map the given handle into the kernel and return a kernel address that
 * can be used to access this address. If no flags are specified, this
 * will return a non-secure uncached mapping.
 */
void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_unmap_kernel() - destroy a kernel mapping for a handle
 * @client:	the client
 * @handle:	handle to unmap
 */
void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_share_dma_buf() - share buffer as dma-buf
 * @client:	the client
 * @handle:	the handle
 */
struct dma_buf *ion_share_dma_buf(struct ion_client *client,
						struct ion_handle *handle);

/**
 * ion_share_dma_buf_fd() - given an ion client, create a dma-buf fd
 * @client:	the client
 * @handle:	the handle
 */
int ion_share_dma_buf_fd(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_import_dma_buf() - given an dma-buf fd from the ion exporter get handle
 * @client:	the client
 * @fd:		the dma-buf fd
 *
 * Given an dma-buf fd that was allocated through ion via ion_share_dma_buf,
 * import that fd and return a handle representing it.  If a dma-buf from
 * another exporter is passed in this function will return ERR_PTR(-EINVAL)
 */
struct ion_handle *ion_import_dma_buf(struct ion_client *client, int fd);

/**
 * ion_handle_get_flags - get the flags for a given handle
 *
 * @client - client who allocated the handle
 * @handle - handle to get the flags
 * @flags - pointer to store the flags
 *
 * Gets the current flags for a handle. These flags indicate various options
 * of the buffer (caching, security, etc.)
 */
int ion_handle_get_flags(struct ion_client *client, struct ion_handle *handle,
				unsigned long *flags);


/**
 * ion_map_iommu - map the given handle into an iommu
 *
 * @client - client who allocated the handle
 * @handle - handle to map
 * @domain_num - domain number to map to
 * @partition_num - partition number to allocate iova from
 * @align - alignment for the iova
 * @iova_length - length of iova to map. If the iova length is
 *		greater than the handle length, the remaining
 *		address space will be mapped to a dummy buffer.
 * @iova - pointer to store the iova address
 * @buffer_size - pointer to store the size of the buffer
 * @flags - flags for options to map
 * @iommu_flags - flags specific to the iommu.
 *
 * Maps the handle into the iova space specified via domain number. Iova
 * will be allocated from the partition specified via partition_num.
 * Returns 0 on success, negative value on error.
 */
int ion_map_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, unsigned long *iova,
			unsigned long *buffer_size,
			unsigned long flags, unsigned long iommu_flags);


/**
 * ion_handle_get_size - get the allocated size of a given handle
 *
 * @client - client who allocated the handle
 * @handle - handle to get the size
 * @size - pointer to store the size
 *
 * gives the allocated size of a handle. returns 0 on success, negative
 * value on error
 *
 * NOTE: This is intended to be used only to get a size to pass to map_iommu.
 * You should *NOT* rely on this for any other usage.
 */

int ion_handle_get_size(struct ion_client *client, struct ion_handle *handle,
			unsigned long *size);

/**
 * ion_unmap_iommu - unmap the handle from an iommu
 *
 * @client - client who allocated the handle
 * @handle - handle to unmap
 * @domain_num - domain to unmap from
 * @partition_num - partition to unmap from
 *
 * Decrement the reference count on the iommu mapping. If the count is
 * 0, the mapping will be removed from the iommu.
 */
void ion_unmap_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num);


/**
 * ion_secure_heap - secure a heap
 *
 * @client - a client that has allocated from the heap heap_id
 * @heap_id - heap id to secure.
 * @version - version of content protection
 * @data - extra data needed for protection
 *
 * Secure a heap
 * Returns 0 on success
 */
int ion_secure_heap(struct ion_device *dev, int heap_id, int version,
			void *data);

/**
 * ion_unsecure_heap - un-secure a heap
 *
 * @client - a client that has allocated from the heap heap_id
 * @heap_id - heap id to un-secure.
 * @version - version of content protection
 * @data - extra data needed for protection
 *
 * Un-secure a heap
 * Returns 0 on success
 */
int ion_unsecure_heap(struct ion_device *dev, int heap_id, int version,
			void *data);

/**
 * msm_ion_do_cache_op - do cache operations.
 *
 * @client - pointer to ION client.
 * @handle - pointer to buffer handle.
 * @vaddr -  virtual address to operate on.
 * @len - Length of data to do cache operation on.
 * @cmd - Cache operation to perform:
 *		ION_IOC_CLEAN_CACHES
 *		ION_IOC_INV_CACHES
 *		ION_IOC_CLEAN_INV_CACHES
 *
 * Returns 0 on success
 */
int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd);



struct ion_handle *ion_dma_buf_to_handle(struct ion_client *client,
					 struct dma_buf *dmabuf);
#else
static inline void ion_reserve(struct ion_platform_data *data)
{

}

static inline struct ion_client *ion_client_create(struct ion_device *dev,
				     unsigned int heap_mask, const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline void ion_client_destroy(struct ion_client *client) { }

static inline struct ion_handle *ion_alloc(struct ion_client *client,
					size_t len, size_t align,
					unsigned int heap_mask,
					unsigned int flags)
{
	return ERR_PTR(-ENODEV);
}

static inline void ion_free(struct ion_client *client,
	struct ion_handle *handle) { }


static inline int ion_phys(struct ion_client *client,
	struct ion_handle *handle, ion_phys_addr_t *addr, size_t *len)
{
	return -ENODEV;
}

static inline struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle)
{
	return ERR_PTR(-ENODEV);
}

static inline void *ion_map_kernel(struct ion_client *client,
	struct ion_handle *handle, unsigned long flags)
{
	return ERR_PTR(-ENODEV);
}

static inline void ion_unmap_kernel(struct ion_client *client,
	struct ion_handle *handle) { }

static inline int ion_share_dma_buf(struct ion_client *client, struct ion_handle *handle)
{
	return -ENODEV;
}

static inline struct ion_handle *ion_import_dma_buf(struct ion_client *client, int fd)
{
	return ERR_PTR(-ENODEV);
}

static inline int ion_handle_get_flags(struct ion_client *client,
	struct ion_handle *handle, unsigned long *flags)
{
	return -ENODEV;
}

static inline int ion_map_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num, unsigned long align,
			unsigned long iova_length, unsigned long *iova,
			unsigned long *buffer_size,
			unsigned long flags,
			unsigned long iommu_flags)
{
	return -ENODEV;
}

static inline void ion_unmap_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num)
{
	return;
}

static inline int ion_secure_heap(struct ion_device *dev, int heap_id,
					int version, void *data)
{
	return -ENODEV;

}

static inline int ion_unsecure_heap(struct ion_device *dev, int heap_id,
					int version, void *data)
{
	return -ENODEV;
}

static inline int msm_ion_do_cache_op(struct ion_client *client,
			struct ion_handle *handle, void *vaddr,
			unsigned long len, unsigned int cmd)
{
	return -ENODEV;
}

static inline struct ion_handle *ion_dma_buf_to_handle(
					struct ion_client *client,
					struct dma_buf *dmabuf)
{
	return ERR_PTR(-ENODEV);
}


#endif /* CONFIG_ION */
#endif /* __KERNEL__ */

/**
 * DOC: Ion Userspace API
 *
 * create a client by opening /dev/ion
 * most operations handled via following ioctls
 *
 */

/**
 * struct ion_allocation_data - metadata passed from userspace for allocations
 * @len:	size of the allocation
 * @align:	required alignment of the allocation
 * @heap_mask:	mask of heaps to allocate from
 * @flags:	flags passed to heap
 * @handle:	pointer that will be populated with a cookie to use to refer
 *		to this allocation
 *
 * Provided by userspace as an argument to the ioctl
 */
struct ion_allocation_data {
	size_t len;
	size_t align;
	unsigned int heap_mask;
	unsigned int flags;
	struct ion_handle *handle;
};

/**
 * struct ion_fd_data - metadata passed to/from userspace for a handle/fd pair
 * @handle:	a handle
 * @fd:		a file descriptor representing that handle
 *
 * For ION_IOC_SHARE or ION_IOC_MAP userspace populates the handle field with
 * the handle returned from ion alloc, and the kernel returns the file
 * descriptor to share or map in the fd field.  For ION_IOC_IMPORT, userspace
 * provides the file descriptor and the kernel returns the handle.
 */
struct ion_fd_data {
	struct ion_handle *handle;
	int fd;
};

/**
 * struct ion_handle_data - a handle passed to/from the kernel
 * @handle:	a handle
 */
struct ion_handle_data {
	struct ion_handle *handle;
};

/**
 * struct ion_custom_data - metadata passed to/from userspace for a custom ioctl
 * @cmd:	the custom ioctl function to call
 * @arg:	additional data to pass to the custom ioctl, typically a user
 *		pointer to a predefined structure
 *
 * This works just like the regular cmd and arg fields of an ioctl.
 */
struct ion_custom_data {
	unsigned int cmd;
	unsigned long arg;
};
#define ION_IOC_MAGIC		'I'

/**
 * DOC: ION_IOC_ALLOC - allocate memory
 *
 * Takes an ion_allocation_data struct and returns it with the handle field
 * populated with the opaque handle for the allocation.
 */
#define ION_IOC_ALLOC		_IOWR(ION_IOC_MAGIC, 0, \
				      struct ion_allocation_data)

/**
 * DOC: ION_IOC_FREE - free memory
 *
 * Takes an ion_handle_data struct and frees the handle.
 */
#define ION_IOC_FREE		_IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)

/**
 * DOC: ION_IOC_MAP - get a file descriptor to mmap
 *
 * Takes an ion_fd_data struct with the handle field populated with a valid
 * opaque handle.  Returns the struct with the fd field set to a file
 * descriptor open in the current address space.  This file descriptor
 * can then be used as an argument to mmap.
 */
#define ION_IOC_MAP		_IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)

/**
 * DOC: ION_IOC_SHARE - creates a file descriptor to use to share an allocation
 *
 * Takes an ion_fd_data struct with the handle field populated with a valid
 * opaque handle.  Returns the struct with the fd field set to a file
 * descriptor open in the current address space.  This file descriptor
 * can then be passed to another process.  The corresponding opaque handle can
 * be retrieved via ION_IOC_IMPORT.
 */
#define ION_IOC_SHARE		_IOWR(ION_IOC_MAGIC, 4, struct ion_fd_data)

/**
 * DOC: ION_IOC_IMPORT - imports a shared file descriptor
 *
 * Takes an ion_fd_data struct with the fd field populated with a valid file
 * descriptor obtained from ION_IOC_SHARE and returns the struct with the handle
 * filed set to the corresponding opaque handle.
 */
#define ION_IOC_IMPORT		_IOWR(ION_IOC_MAGIC, 5, struct ion_fd_data)

/**
 * DOC: ION_IOC_CUSTOM - call architecture specific ion ioctl
 *
 * Takes the argument of the architecture specific ioctl to call and
 * passes appropriate userdata for that ioctl
 */
#define ION_IOC_CUSTOM		_IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data)


#endif /* _LINUX_ION_H */
