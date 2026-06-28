#!/usr/bin/env python3
import struct
import os

SECTOR_SIZE = 512

def read_sectors(f, start, count=1):
    f.seek(start * SECTOR_SIZE)
    return bytearray(f.read(count * SECTOR_SIZE))

def write_sectors(f, start, data):
    f.seek(start * SECTOR_SIZE)
    f.write(data)

def get_fat_info(img_path, partition_offset):
    with open(img_path, 'rb') as f:
        f.seek(partition_offset)
        boot = bytearray(f.read(512))
    
    bytes_per_sec = struct.unpack_from('<H', boot, 0x0B)[0]
    sec_per_cluster = boot[0x0D]
    reserved_secs = struct.unpack_from('<H', boot, 0x0E)[0]
    num_fats = boot[0x10]
    if num_fats == 0:
        num_fats = 2
    fat_size = struct.unpack_from('<I', boot, 0x24)[0]
    root_cluster = struct.unpack_from('<I', boot, 0x2C)[0]
    total_secs = struct.unpack_from('<I', boot, 0x20)[0]
    if total_secs == 0:
        total_secs = struct.unpack_from('<Q', boot, 0x20)[0]
    
    fat_start_sec = reserved_secs
    data_start_sec = fat_start_sec + num_fats * fat_size
    cluster_size = bytes_per_sec * sec_per_cluster
    
    return {
        'bytes_per_sec': bytes_per_sec,
        'sec_per_cluster': sec_per_cluster,
        'reserved_secs': reserved_secs,
        'num_fats': num_fats,
        'fat_size': fat_size,
        'root_cluster': root_cluster,
        'total_secs': total_secs,
        'fat_start_sec': fat_start_sec,
        'data_start_sec': data_start_sec,
        'cluster_size': cluster_size,
        'partition_offset': partition_offset,
    }

class Fat32Image:
    def __init__(self, path, info):
        self.path = path
        self.info = info
        self.po = info['partition_offset']
        self.f = open(path, 'r+b')
    
    def close(self):
        self.f.close()
    
    def sector(self, n):
        self.f.seek(self.po + n * SECTOR_SIZE)
        return self.f
    
    def cluster_offset(self, cluster):
        return self.po + (self.info['data_start_sec'] + (cluster - 2) * self.info['sec_per_cluster']) * SECTOR_SIZE
    
    def read_cluster(self, cluster):
        self.f.seek(self.cluster_offset(cluster))
        return bytearray(self.f.read(self.info['cluster_size']))
    
    def write_cluster(self, cluster, data):
        self.f.seek(self.cluster_offset(cluster))
        self.f.write(data)
    
    def read_fat(self, cluster):
        offset = self.po + self.info['fat_start_sec'] * SECTOR_SIZE + cluster * 4
        self.f.seek(offset)
        return struct.unpack('<I', self.f.read(4))[0] & 0x0FFFFFFF
    
    def write_fat(self, cluster, value):
        offset = self.po + self.info['fat_start_sec'] * SECTOR_SIZE + cluster * 4
        v = value & 0x0FFFFFFF
        # Write to all FAT copies
        for i in range(self.info['num_fats']):
            off = offset + i * self.info['fat_size'] * SECTOR_SIZE
            self.f.seek(off)
            self.f.write(struct.pack('<I', v))
    
    def alloc_clusters(self, count):
        total_data_secs = self.info['total_secs'] - self.info['data_start_sec']
        num_clusters = total_data_secs // self.info['sec_per_cluster']
        clusters = []
        c = 2
        while len(clusters) < count and c < num_clusters + 2:
            v = self.read_fat(c)
            if v == 0:
                clusters.append(c)
            c += 1
        return clusters if len(clusters) == count else None
    
    def find_dir_entry(self, dir_cluster, name_short):
        buf = self.read_cluster(dir_cluster)
        for i in range(len(buf) // 32):
            entry = buf[i*32:(i+1)*32]
            attr = entry[0x0B]
            if entry[0] == 0x00:
                return i, buf
            if entry[0] == 0xE5:
                continue
            if attr == 0x0F or attr == 0x08:
                continue
            if entry[0:11] == name_short:
                return i, buf
        return -1, buf

    def find_free_entry(self, dir_cluster):
        buf = self.read_cluster(dir_cluster)
        for i in range(len(buf) // 32):
            entry = buf[i*32:(i+1)*32]
            if entry[0] == 0x00 or entry[0] == 0xE5:
                return i, buf
        return -1, buf
    
    def write_dir_entry(self, dir_cluster, index, entry_data, buf):
        buf[index*32:(index+1)*32] = entry_data
        self.write_cluster(dir_cluster, buf)
    
    def add_directory(self, parent_cluster, name_short, name_long=None):
        sub_clusters = self.alloc_clusters(1)
        if not sub_clusters:
            return None
        
        cluster = sub_clusters[0]
        for i in range(len(sub_clusters) - 1):
            self.write_fat(sub_clusters[i], sub_clusters[i+1])
        self.write_fat(sub_clusters[-1], 0x0FFFFFFF)
        
        empty_dir = bytearray(self.info['cluster_size'])
        # Dot entry
        dot = bytearray(32)
        dot[0:11] = b'.          '
        dot[0x0B] = 0x10  # Directory attribute
        struct.pack_into('<H', dot, 0x1A, cluster & 0xFFFF)
        struct.pack_into('<H', dot, 0x14, (cluster >> 16) & 0xFFFF)
        empty_dir[0:32] = dot
        
        # Dot-dot entry
        dotdot = bytearray(32)
        dotdot[0:11] = b'..         '
        dotdot[0x0B] = 0x10
        struct.pack_into('<H', dotdot, 0x1A, parent_cluster & 0xFFFF)
        struct.pack_into('<H', dotdot, 0x14, (parent_cluster >> 16) & 0xFFFF)
        empty_dir[32:64] = dotdot
        
        self.write_cluster(cluster, empty_dir)
        
        # Add entry to parent
        idx, buf = self.find_dir_entry(parent_cluster, name_short)
        if idx >= 0 and buf[idx*32] != 0:
            attr = buf[idx*32 + 0x0B]
            if attr & 0x10:
                return cluster
        
        idx, buf = self.find_free_entry(parent_cluster)
        if idx < 0:
            print(f"No free entry in cluster {parent_cluster}")
            return None
        
        entry = bytearray(32)
        entry[0:11] = name_short
        entry[0x0B] = 0x10  # Directory
        struct.pack_into('<H', entry, 0x1A, cluster & 0xFFFF)
        struct.pack_into('<H', entry, 0x14, (cluster >> 16) & 0xFFFF)
        self.write_dir_entry(parent_cluster, idx, entry, buf)
        
        return cluster
    
    def add_file(self, parent_cluster, name_short, data):
        if len(data) == 0:
            return False
        
        cluster_size = self.info['cluster_size']
        num_clusters = (len(data) + cluster_size - 1) // cluster_size
        clusters = self.alloc_clusters(num_clusters)
        if not clusters:
            print(f"Not enough free clusters for {name_short}")
            return False
        
        for i, c in enumerate(clusters):
            if i < len(clusters) - 1:
                self.write_fat(c, clusters[i+1])
            else:
                self.write_fat(c, 0x0FFFFFFF)
            
            offset = self.cluster_offset(c)
            chunk = data[i * cluster_size:(i+1) * cluster_size]
            self.f.seek(offset)
            self.f.write(chunk)
        
        entry = bytearray(32)
        entry[0:11] = name_short
        entry[0x0B] = 0x20  # Archive
        struct.pack_into('<H', entry, 0x1A, clusters[0] & 0xFFFF)
        struct.pack_into('<H', entry, 0x14, (clusters[0] >> 16) & 0xFFFF)
        struct.pack_into('<H', entry, 0x1C, len(data) & 0xFFFF)
        struct.pack_into('<H', entry, 0x1E, (len(data) >> 16) & 0xFFFF)
        
        idx, buf = self.find_free_entry(parent_cluster)
        if idx < 0:
            print(f"No free directory entry for {name_short}")
            return False
        
        self.write_dir_entry(parent_cluster, idx, entry, buf)
        return True
    
    def mkdirs(self, root_cluster, path):
        parts = path.split('/')
        current = root_cluster
        for part in parts:
            if not part:
                continue
            name_short = part.upper()[:8].ljust(8, ' ').encode('ascii') + b'   '
            idx, buf = self.find_dir_entry(current, name_short)
            if idx >= 0 and buf[idx*32] != 0 and buf[idx*32+0x0B] & 0x10:
                entry = buf[idx*32:(idx+1)*32]
                current = struct.unpack_from('<H', entry, 0x1A)[0] | (struct.unpack_from('<H', entry, 0x14)[0] << 16)
            else:
                current = self.add_directory(current, name_short)
                if not current:
                    return None
        return current

def main():
    build_dir = '/home/iskra/NeoDOS/build'
    img_path = os.path.join(build_dir, 'disk.img')
    partition_offset = 1048576
    
    info = get_fat_info(img_path, partition_offset)
    print(f"FAT32: cluster={info['cluster_size']}, data_start={info['data_start_sec']}sec")
    
    fat = Fat32Image(img_path, info)
    
    root = info['root_cluster']
    
    efi_dir = fat.mkdirs(root, 'EFI/BOOT')
    neodos_dir = fat.mkdirs(root, 'NEODOS')
    
    print(f"EFI/BOOT cluster: {efi_dir}, NEODOS cluster: {neodos_dir}")
    
    with open(os.path.join(build_dir, 'BOOTX64.EFI'), 'rb') as f:
        data = f.read()
    fat.add_file(efi_dir, b'BOOTX64 EFI', data)
    print(f"Added BOOTX64.EFI ({len(data)} bytes)")
    
    with open(os.path.join(build_dir, 'OSDATA.NDR'), 'rb') as f:
        data = f.read()
    fat.add_file(neodos_dir, b'OSDATA  NDR', data)
    print(f"Added OSDATA.NDR ({len(data)} bytes)")
    
    with open(os.path.join(build_dir, 'NEOKRN.ELF'), 'rb') as f:
        data = f.read()
    fat.add_file(neodos_dir, b'NEOKRN  ELF', data)
    print(f"Added NEOKRN.ELF ({len(data)} bytes)")
    
    with open(os.path.join(build_dir, 'FONT.NFF'), 'rb') as f:
        data = f.read()
    fat.add_file(neodos_dir, b'FONT    NFF', data)
    print(f"Added FONT.NFF ({len(data)} bytes)")
    
    fat.close()
    print("Done!")

if __name__ == '__main__':
    main()
