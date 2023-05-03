// +--------------------------------------------------------------
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  Description: This file contains the C# object representation of Info.xml that InMage generates.
//
//  This file started off as a generated file and then tweaks on top of it.
//
// ---------------------------------------------------------------

namespace VMware.Client
{
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    [System.Xml.Serialization.XmlRootAttribute(Namespace = "", IsNullable = false, ElementName = "INFO")]
    public partial class InfrastructureView
    {
        private INFOConfig[] configField;
        private INFODatastore[] datastoreField;
        private INFOHost[] hostField;
        private INFONetwork[] networkField;
        private INFODataCenter[] datacenterField;
        private INFOResourcepool[] resourcepoolField;
        private string vCenterField;
        private string versionField;

        /// <remarks/>
        [System.Xml.Serialization.XmlElementAttribute("config", Order = 5)]
        public INFOConfig[] config
        {
            get
            {
                return this.configField;
            }
            set
            {
                this.configField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlElementAttribute("datastore", Order = 2)]
        public INFODatastore[] datastore
        {
            get
            {
                return this.datastoreField;
            }
            set
            {
                this.datastoreField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlElementAttribute("datacenter", Order = 6)]
        public INFODataCenter[] datacenter
        {
            get
            {
                return this.datacenterField;
            }
            set
            {
                this.datacenterField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlElementAttribute("host", Order = 1)]
        public INFOHost[] host
        {
            get
            {
                return this.hostField;
            }
            set
            {
                this.hostField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlElementAttribute("network", Order = 3)]
        public INFONetwork[] network
        {
            get
            {
                return this.networkField;
            }
            set
            {
                this.networkField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlElementAttribute("resourcepool", Order = 4)]
        public INFOResourcepool[] resourcepool
        {
            get
            {
                return this.resourcepoolField;
            }
            set
            {
                this.resourcepoolField = value;
            }
        }
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vCenter
        {
            get
            {
                return this.vCenterField;
            }
            set
            {
                this.vCenterField = value;
            }
        }
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string version
        {
            get
            {
                return this.versionField;
            }
            set
            {
                this.versionField = value;
            }
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    public partial class INFOConfig
    {
        private short cpucountField;
        private string memsizeField;
        private string vSpherehostnameField;
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public short cpucount
        {
            get
            {
                return this.cpucountField;
            }
            set
            {
                this.cpucountField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string memsize
        {
            get
            {
                return this.memsizeField;
            }
            set
            {
                this.memsizeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vSpherehostname
        {
            get
            {
                return this.vSpherehostnameField;
            }
            set
            {
                this.vSpherehostnameField = value;
            }
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    public partial class INFODataCenter
    {
        private string datacenter_nameField;
        private string refIdField;

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string datacenter_name
        {
            get
            {
                return this.datacenter_nameField;
            }
            set
            {
                this.datacenter_nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string refid
        {
            get
            {
                return this.refIdField;
            }
            set
            {
                this.refIdField = value;
            }
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    public partial class INFODatastore
    {
        private int datastore_blocksize_mbField;
        private string datastore_nameField;

        private string disk_nameField;
        private string display_nameField;
        private string filesystem_versionField;
        private string free_spaceField;
        private string lunField;
        private string total_sizeField;
        private string typeField;
        private string uuidField;
        private string vendorField;
        private string vSpherehostnameField;
        private string mount_pathField;
        private string refidField;
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public int datastore_blocksize_mb
        {
            get
            {
                return this.datastore_blocksize_mbField;
            }
            set
            {
                this.datastore_blocksize_mbField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string datastore_name
        {
            get
            {
                return this.datastore_nameField;
            }
            set
            {
                this.datastore_nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string disk_name
        {
            get
            {
                return this.disk_nameField;
            }
            set
            {
                this.disk_nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string display_name
        {
            get
            {
                return this.display_nameField;
            }
            set
            {
                this.display_nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string filesystem_version
        {
            get
            {
                return this.filesystem_versionField;
            }
            set
            {
                this.filesystem_versionField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string free_space
        {
            get
            {
                return this.free_spaceField;
            }
            set
            {
                this.free_spaceField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string lun
        {
            get
            {
                return this.lunField;
            }
            set
            {
                this.lunField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string total_size
        {
            get
            {
                return this.total_sizeField;
            }
            set
            {
                this.total_sizeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string type
        {
            get
            {
                return this.typeField;
            }
            set
            {
                this.typeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string uuid
        {
            get
            {
                return this.uuidField;
            }
            set
            {
                this.uuidField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vendor
        {
            get
            {
                return this.vendorField;
            }
            set
            {
                this.vendorField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vSpherehostname
        {
            get
            {
                return this.vSpherehostnameField;
            }
            set
            {
                this.vSpherehostnameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string mount_path
        {
            get
            {
                return this.mount_pathField;
            }
            set
            {
                this.mount_pathField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string refid
        {
            get
            {
                return this.refidField;
            }
            set
            {
                this.refidField = value;
            }
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    public partial class INFOHost
    {
        private string alt_guest_nameField;
        private string clusterField;
        private string cpucountField;
        private string datacenterField;
        private string datacenter_refidField;
        private string datastoreField;
        private string diskenableuuidField;
        private INFOHostDisk[] diskField;

        private string display_nameField;
        private bool efiField;
        private string floppy_device_countField;
        private string folder_pathField;
        private string hostnameField;
        private string hostversionField;
        private string ide_countField;
        private string ip_addressField;
        private string memsizeField;
        private INFOHostNic[] nicField;
        private int number_of_disksField;
        private string operatingsystemField;
        private string os_infoField;
        private string powered_statusField;
        private string rdmField;
        private string resourcepoolField;
        private string resourcepoolgrpnameField;
        private string source_uuidField;
        private string source_vc_idField;
        private bool templateField;
        private string vm_console_urlField;
        private string vmwaretoolsField;
        private string vmx_pathField;
        private string vmx_versionField;
        private string vSpherehostnameField;
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string alt_guest_name
        {
            get
            {
                return this.alt_guest_nameField;
            }
            set
            {
                this.alt_guest_nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string cluster
        {
            get
            {
                return this.clusterField;
            }
            set
            {
                this.clusterField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string cpucount
        {
            get
            {
                return this.cpucountField;
            }
            set
            {
                this.cpucountField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string datacenter
        {
            get
            {
                return this.datacenterField;
            }
            set
            {
                this.datacenterField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string datacenter_refid
        {
            get
            {
                return this.datacenter_refidField;
            }
            set
            {
                this.datacenter_refidField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string datastore
        {
            get
            {
                return this.datastoreField;
            }
            set
            {
                this.datastoreField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlElementAttribute("disk")]
        public INFOHostDisk[] disk
        {
            get
            {
                return this.diskField;
            }
            set
            {
                this.diskField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string diskenableuuid
        {
            get
            {
                return this.diskenableuuidField;
            }
            set
            {
                this.diskenableuuidField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string display_name
        {
            get
            {
                return this.display_nameField;
            }
            set
            {
                this.display_nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public bool efi
        {
            get
            {
                return this.efiField;
            }
            set
            {
                this.efiField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string floppy_device_count
        {
            get
            {
                return this.floppy_device_countField;
            }
            set
            {
                this.floppy_device_countField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string folder_path
        {
            get
            {
                return this.folder_pathField;
            }
            set
            {
                this.folder_pathField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string hostname
        {
            get
            {
                return this.hostnameField;
            }
            set
            {
                this.hostnameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string hostversion
        {
            get
            {
                return this.hostversionField;
            }
            set
            {
                this.hostversionField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string ide_count
        {
            get
            {
                return this.ide_countField;
            }
            set
            {
                this.ide_countField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string ip_address
        {
            get
            {
                return this.ip_addressField;
            }
            set
            {
                this.ip_addressField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string memsize
        {
            get
            {
                return this.memsizeField;
            }
            set
            {
                this.memsizeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlElementAttribute("nic")]
        public INFOHostNic[] nic
        {
            get
            {
                return this.nicField;
            }
            set
            {
                this.nicField = value;
            }
        }
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public int number_of_disks
        {
            get
            {
                return this.number_of_disksField;
            }
            set
            {
                this.number_of_disksField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string operatingsystem
        {
            get
            {
                return this.operatingsystemField;
            }
            set
            {
                this.operatingsystemField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string os_info
        {
            get
            {
                return this.os_infoField;
            }
            set
            {
                this.os_infoField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string powered_status
        {
            get
            {
                return this.powered_statusField;
            }
            set
            {
                this.powered_statusField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string rdm
        {
            get
            {
                return this.rdmField;
            }
            set
            {
                this.rdmField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string resourcepool
        {
            get
            {
                return this.resourcepoolField;
            }
            set
            {
                this.resourcepoolField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string resourcepoolgrpname
        {
            get
            {
                return this.resourcepoolgrpnameField;
            }
            set
            {
                this.resourcepoolgrpnameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string source_uuid
        {
            get
            {
                return this.source_uuidField;
            }
            set
            {
                this.source_uuidField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string source_vc_id
        {
            get
            {
                return this.source_vc_idField;
            }
            set
            {
                this.source_vc_idField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public bool template
        {
            get
            {
                return this.templateField;
            }
            set
            {
                this.templateField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vm_console_url
        {
            get
            {
                return this.vm_console_urlField;
            }
            set
            {
                this.vm_console_urlField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vmwaretools
        {
            get
            {
                return this.vmwaretoolsField;
            }
            set
            {
                this.vmwaretoolsField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vmx_path
        {
            get
            {
                return this.vmx_pathField;
            }
            set
            {
                this.vmx_pathField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vmx_version
        {
            get
            {
                return this.vmx_versionField;
            }
            set
            {
                this.vmx_versionField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vSpherehostname
        {
            get
            {
                return this.vSpherehostnameField;
            }
            set
            {
                this.vSpherehostnameField = value;
            }
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    public partial class INFOHostDisk
    {
        private string capacity_in_bytesField;
        private string cluster_diskField;
        private string controller_modeField;
        private string controller_typeField;
        private string disk_modeField;
        private string disk_nameField;

        private string disk_object_idField;
        private string disk_typeField;
        private string ide_or_scsiField;
        private string independent_persistentField;
        private string scsi_mapping_hostField;
        private string scsi_mapping_vmxField;
        private long sizeField;
        private string source_disk_uuidField;
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string capacity_in_bytes
        {
            get
            {
                return this.capacity_in_bytesField;
            }
            set
            {
                this.capacity_in_bytesField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string cluster_disk
        {
            get
            {
                return this.cluster_diskField;
            }
            set
            {
                this.cluster_diskField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string controller_mode
        {
            get
            {
                return this.controller_modeField;
            }
            set
            {
                this.controller_modeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string controller_type
        {
            get
            {
                return this.controller_typeField;
            }
            set
            {
                this.controller_typeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string disk_mode
        {
            get
            {
                return this.disk_modeField;
            }
            set
            {
                this.disk_modeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string disk_name
        {
            get
            {
                return this.disk_nameField;
            }
            set
            {
                this.disk_nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string disk_object_id
        {
            get
            {
                return this.disk_object_idField;
            }
            set
            {
                this.disk_object_idField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string disk_type
        {
            get
            {
                return this.disk_typeField;
            }
            set
            {
                this.disk_typeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string ide_or_scsi
        {
            get
            {
                return this.ide_or_scsiField;
            }
            set
            {
                this.ide_or_scsiField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string independent_persistent
        {
            get
            {
                return this.independent_persistentField;
            }
            set
            {
                this.independent_persistentField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string scsi_mapping_host
        {
            get
            {
                return this.scsi_mapping_hostField;
            }
            set
            {
                this.scsi_mapping_hostField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string scsi_mapping_vmx
        {
            get
            {
                return this.scsi_mapping_vmxField;
            }
            set
            {
                this.scsi_mapping_vmxField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public long size
        {
            get
            {
                return this.sizeField;
            }
            set
            {
                this.sizeField = value;
            }
        }
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string source_disk_uuid
        {
            get
            {
                return this.source_disk_uuidField;
            }
            set
            {
                this.source_disk_uuidField = value;
            }
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    public partial class INFOHostNic
    {
        private string adapter_typeField;
        private string address_typeField;
        private string dhcpField;
        private string dnsipField;
        private string ipField;
        private string network_labelField;

        private string network_nameField;

        private string nic_macField;
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string adapter_type
        {
            get
            {
                return this.adapter_typeField;
            }
            set
            {
                this.adapter_typeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string address_type
        {
            get
            {
                return this.address_typeField;
            }
            set
            {
                this.address_typeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string dhcp
        {
            get
            {
                return this.dhcpField;
            }
            set
            {
                this.dhcpField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string dnsip
        {
            get
            {
                return this.dnsipField;
            }
            set
            {
                this.dnsipField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string ip
        {
            get
            {
                return this.ipField;
            }
            set
            {
                this.ipField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string network_label
        {
            get
            {
                return this.network_labelField;
            }
            set
            {
                this.network_labelField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string network_name
        {
            get
            {
                return this.network_nameField;
            }
            set
            {
                this.network_nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string nic_mac
        {
            get
            {
                return this.nic_macField;
            }
            set
            {
                this.nic_macField = value;
            }
        }
    }
    /// <remarks/>
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    public partial class INFONetwork
    {
        private string nameField;
        private string portgroupKeyField;
        private string switchUuidField;
        private string vSpherehostnameField;
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string name
        {
            get
            {
                return this.nameField;
            }
            set
            {
                this.nameField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string portgroupKey
        {
            get
            {
                return this.portgroupKeyField;
            }
            set
            {
                this.portgroupKeyField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string switchUuid
        {
            get
            {
                return this.switchUuidField;
            }
            set
            {
                this.switchUuidField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string vSpherehostname
        {
            get
            {
                return this.vSpherehostnameField;
            }
            set
            {
                this.vSpherehostnameField = value;
            }
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true)]
    public partial class INFOResourcepool
    {
        private string groupIdField;
        private string hostField;

        private string nameField;
        private string owner_typeField;
        private string ownerField;
        private string typeField;
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string groupId
        {
            get
            {
                return this.groupIdField;
            }
            set
            {
                this.groupIdField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string host
        {
            get
            {
                return this.hostField;
            }
            set
            {
                this.hostField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string name
        {
            get
            {
                return this.nameField;
            }
            set
            {
                this.nameField = value;
            }
        }
        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string owner
        {
            get
            {
                return this.ownerField;
            }
            set
            {
                this.ownerField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string owner_type
        {
            get
            {
                return this.owner_typeField;
            }
            set
            {
                this.owner_typeField = value;
            }
        }

        /// <remarks/>
        [System.Xml.Serialization.XmlAttributeAttribute()]
        public string type
        {
            get
            {
                return this.typeField;
            }
            set
            {
                this.typeField = value;
            }
        }
    }
}