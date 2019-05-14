# Running NDT on Amazon Web Services

This document provides a minimal set of instructions for getting NDT running
on an Amazon Web Services HPC cluster.
To keep these instructions as simple as possible, many common practices
especially with respect to security have been left out.
Use at your own risk.
These instructions were last verified to work on 2019-05-13.

---

### Prerequisites

 * [Amazon Web Services](https://console.aws.amazon.com/console/home) Account ([create](https://portal.aws.amazon.com/billing/signup))
 * [Python](https://www.python.org/downloads/) (v2.7 or v3.4 or higher)
 * [Pip](https://pip.pypa.io/en/stable/installing/)

---

### Preparation

Since pip installs executables to an odd location, update the `PATH` to
include this location.

```text
$ export PATH="~/.local/bin:$PATH"
```

To make this change persistent, runtime configuration files will need to be updated as well (e.g., in bash, add `export PATH="~/.local/bin:$PATH"` to the end of `~/.bashrc`).

#### Creating a User

Before a cluster can be set up, a user must be created with permissions to
create the cluster.

1. Open the [IAM dashboard](https://console.aws.amazon.com/iam/).
2. Click "Users" in the left-nav.
3. Click "Add user" button at the top of page.
4. Add User:
   1. Enter a username (e.g. `test_user`).
   2. Select the "Programmatic access" checkbox.
   3. Click "Next: Permissions" at the bottom of the page.
5. Set permissions:
   1. Select "Attach existing policies directly" near the top.
   2. Select the checkbox next to "AdministratorAccess".
   3. Click "Next: Tags" at the bottom of the page.
6. Click "Next: Review" at the bottom of the page.
7. Screen should show:
    * User name that you entered
    * Programmatic access - with an access key
    * Permissions boundary is not set
    * Managed policy: AdministratorAccess
8. If all is correct, click "Create User" at the bottom of the page.
9. Click "Show" under "Secret access key".
10. Record the Access key ID which will be needed later.
11. Record Secret access key which will be needed later.</br>**Warning: This is the last chance you will have to get this secret key.**
12. Click "Download .csv" to get the username and keys in CSV format.  *(optional)*
13. Click "Close" button at the bottom of the page.

If you failed to record or otherwise misplaced the secret key, a new key pair can be created for a user.

1. Open [IAM dashboard](https://console.aws.amazon.com/iam/).
2. Click "Users" in the left-nav.
3. Click on the User name of the user that needs a new key pair.
4. Select the "Security credentials" tab.
5. Click the "Create access key" button in the "Access Keys" section near the middle of the page.
6. Click "Show" under "Secret access key".
7. Record the Access key ID which is needed later.
8. Record the Secret access key which will be needed later.</br>**Warning: This is the last chance you will have to get this secret key.**
9. Click "Download .csv" to get the User name and keys in CSV format.  *(optional)*
10. Click "Close" button.

#### Creating an SSH key pair

In order to login to a cluster via `ssh`, a pair of ssh keys is needed.
To create such a pair:

1. Open [EC2 dashboard](https://console.aws.amazon.com/ec2/v2/).
2. Click the "+" to the left of "NETWORK & SECURITY" in the left-nav, if it is not already open.
3. Click on "Key Pairs" in the left-nav under "NETWORK & SECURITY".
4. Click the "Create Key Pair" button at the top.
5. Give the key a meaningful name (e.g., TestClusterSshKey).
6. Click the "Create" button, which will download a PEM file (possibly with a
`.txt` extension).
7. Move the downloaded file to your ssh configuration directory (e.g., `mv ~/Downloads/TestClusterSshKey.pem.txt ~/.ssh/TestClusterSshKey.pem`)
8. Set permissions on the PEM file to be only readable by owner (e.g., `chmod 400 ~/.ssh/TestClusterSshKey.pem`). 

#### Install AWS ParallelCluster

Cluster setup will be managed with the `pcluster` command line interface (CLI) which can be installed via `pip`.
```text
$ pip install --user --upgrade awscli
$ pip install --user --upgrade aws-parallelcluster
```
**Note: The utility formerly known as [CfnCluster](https://cfncluster.readthedocs.io/en/latest/getting_started.html) was [renamed](https://github.com/aws/aws-parallelcluster/commit/eebd1029846ddea7eda00505d482bc83395890bb) to `pcluster`.**

Next the CLI needs to be configured.
```text
$ aws configure
AWS Access Key ID [None]: <Access key ID from user creation>
AWS Secret Access Key [None]: <Secret access key from user creation>
Default region name [None]: us-east-2
Default output format [None]: text
```

If all went well, this should have created `~/.aws/config` and `~/.aws/credentials`.

---

### Creating Cluster Template

Configure a cluster template by running `pcluster configure`.

The process should look something like this:
```text
$ pcluster configure
Cluster Template [default]: default
AWS Access Key ID []: <Your Access Key ID>
AWS Secret Access Key ID []: <Your Secret key ID>
Acceptable Values for AWS Region ID:
    eu-north-1
    ap-south-1
    eu-west-3
    eu-west-2
    eu-west-1
    ap-northeast-2
    ap-northeast-1
    sa-east-1
    ca-central-1
    ap-southeast-1
    ap-southeast-2
    eu-central-1
    us-east-1
    us-east-2
    us-west-1
    us-west-2
AWS Region ID []: us-east-2
VPC Name [public]: public
Acceptable Values for Key Name:
    TestClusterSshKey
Key Name []: TestClusterSshKey
Acceptable Values for VPC ID:
    vpc-dfd9c6b7
VPC ID []: vpc-dfd9c6b7
Acceptable Values for Master Subnet ID:
    subnet-5accb120
    subnet-baf1f6d2
    subnet-68ad0924
Master Subnet ID []: subnet-5accb120
```

Configure compute [instance types](https://aws.amazon.com/ec2/instance-types/):
1. Open `~/.parallelcluster/config` in a text editor.
2. Find the section `[cluster default]`.
3. Add (or modify) the `compute_instance_type` value (e.g., add a line `compute_instance_type = t2.micro`).
4. Add (or modify) `initial_queue_size` and/or `max_queue_size`. (e.g., add a line `max_queue_size = 10`) *(optional)*

*Note: If you need more than the default number of nodes (10), go to the
[EC2 Service Limits](https://us-east-2.console.aws.amazon.com/ec2/v2/#Limits)
page to request an increase to the limits for the instance type being used.*

---

### Creating a Cluster

Actually create a cluster:
```text
$ pcluster create my-test-cluster
```
**Note: This will take a surprisingly long time (approximately 5-10 minutes).**

Verify that the cluster exists, by listing the current clusters *(optional)*:
```text
$ pcluster list
```

---

### Acquiring / Compiling NDT

Login to cluster:
```text
$ pcluster ssh my-test-cluster -i ~/.ssh/TestClusterSshKey.pem
```
When prompted about the authenticity of the host, answer `yes`.

Clone NDT:
```text
$ cd /shared
$ git clone https://github.com/doing-science-to-stuff/ndt.git
```

Compile NDT:
```text
$ cd ndt
$ cmake . && make
```

If any additional files are needed (e.g. updated scene files or YAML files),
those can be copied with `scp`.
1. Open the [EC2 dashboard](https://console.aws.amazon.com/ec2/v2/).
2. Click the "+" to the left of "INSTANCES" in the left-nav, if it is not already open.
3. Click on "Instances" in the left-nav under "INSTANCES".
4. Find the row with "Master" as its name.
5. Scroll over to the "IPv4 Public IP" column.
6. Record the public IP address for the Master instance.

Using the public IP address, files can be transferred using `scp`.
```text
$ scp -i ~/.ssh/~/.ssh/TestClusterSshKey.pem \
        ./path_to_local_file \
        ec2-user@a.b.c.d:/shared/ndt/path_to_remote_file
```
Where *a.b.c.d* is the public IP address from the instances table.

---

### Making a Snapshot

To simplify setting up clusters in the future, it is possible to take a
snapshot of the current state of the `/shared` volume on the cluster.
The snapshot can then be used as the starting point for future cluster
instances.

To create a snapshot of the cluster:
1. Open the [EC2 dashboard](https://console.aws.amazon.com/ec2/v2/).
2. Click the "+" to the left of "INSTANCES" in the left-nav, if it is not already open.
3. Click on "Instances" in the left-nav under "INSTANCES".
4. Find and click on the row with "Master" as its name.
5. Select the "Description" tab in the lower section of the page.
6. Find the "Block devices" section of the description.
7. Click on "/dev/sdb".
8. Click on the name of the ESB ID.
9. Click the "Actions" button at the top of the screen.
10. Select "Create Snapshot".
11. Enter a description (e.g., `NDT Compiled and Ready to Run`).
12. Click "Create Snapshot" button.
13. Open the [snapshots console](https://console.aws.amazon.com/ec2/v2/home#Snapshots) to check on the status of the snapshot.

---

### Queuing / Running a Job

The cluster uses a queueing system to manage jobs.
To submit a job to the queuing system, a submission script that describes what
is to be run must be written.

Using a text editor (e.g., `nano` or `vim`), create a file named `example_job.sh`.
In that file add the following text:
```text
#!/bin/sh
#$ -cwd
#$ -N ndt
#$ -pe mpi 10
#$ -j y
/usr/lib64/mpich/bin/mpirun -np $NSLOTS ./ndt -b r -f 3 -d 4 -s scenes/hypercube.so
```
In this file the `10` in the line `#$ -pe mpi 10` specified how many *slots*
(cores) the job will use.

Submit the job:
```text
$ qsub example_job.sh
```
*Note: It may complain that `ec2-user's job is not allowed to run in any
queue`.  This can be ignored.*

The status of the submitted job can be tracked with the `qstat` command.
```text
[ec2-user@ip-172-31-21-122 ndt]$ qstat
job-ID  prior   name       user         state submit/start at     queue         slots ja-task-ID
------------------------------------------------------------------------------------------------
      1 0.55500 ndt        ec2-user     qw    05/13/2019 21:44:55                  10
```

The state `qw` means the job is waiting for enough resources to be available
to run the job.

Progress on adding execution hosts to the cluster can be tracked with the
`qstat` command.
```text
$ qstat -sh
```

When the job starts, the state will change to `r`.
Once the job completes, it will no longer show up in `qstat` output.

Standard out is redirected to a file, progress of the running job can be
tracked by tailing this file.
Since the job-ID show by `qstat` is `1`, the filename will be `ndt.o1`.
```text
$ tail -f ndt.o1
```
To stop monitoring the progress, press *ctrl+c*.
This will not affect the running job, it will only stop displaying
of further output.

If you need to stop a running job, you can do so with the `qdel` command.
```text
$ qdel 1
```
Where `1` is the job-ID for the running job, as reported by `qstat`.

---

### Retrieving Results

Once a rendering job is complete, the resulting images will need to be
retrieved from the cluster before the cluster is deleted.

**Warning: Any output not captured in a snapshot or transferred off of the
cluster will be destroyed when the cluster is deleted.**

1. Open the [EC2 dashboard](https://console.aws.amazon.com/ec2/v2/).
2. Click the "+" to the left of "INSTANCES" in the left-nav, if it is not already open.
3. Click on "Instances" in the left-nav under "INSTANCES".
4. Find the row with "Master" as its name.
5. Scroll over to the "IPv4 Public IP" column.
6. Record the IP address for the Master instance.

Using the public IP address, files can be transferred using `scp`.
```text
$ mkdir results
$ scp -r -i ~/.ssh/~/.ssh/TestClusterSshKey.pem \
        ec2-user@a.b.c.d:/shared/ndt/images \
        ./results
```
Where *a.b.c.d* is the public IP address from the instances table.

---

### Deleting a Cluster

When you are done with the cluster, it can be torn down with the `pcluster`
command.
```text
$ pcluster delete my-test-cluster
```

Verify that cluster was deleted, by listing the current clusters *(optional)*:
```text
$ pcluster list
```

---

### Attaching a snapshot

If you created a snapshot before deleting the cluster, you can use that
snapshot as the starting point for future cluster instances.

1. Open the [snapshots console](https://console.aws.amazon.com/ec2/v2/#Snapshots).
2. Locate the "Snapshot ID" of the snapshot you are interested in.
3. Add an `ebs` section to `~/.parallelcluster/config`:
    ```text
    [ebs snapshot_name]
    ebs_snapshot_id = snap-XXXXXXXXXXXXXXXX
    ```
    Where *snap-XXXXXXXXXXXXXXXX* is the actual ID of the snapshot.
4. Add a reference to the snapshot in the `[cluster default]` section:
    ```text
    ebs_settings = snap-XXXXXXXXXXXXXXXX
    ```

At this point rerunning `pcluster create cluster-name` will create a cluster
such that `/shared` is populated with the contents of the snapshot,
eliminating the need to refetch the source code and compile it.


# References:

 * [AWS ParallelCluster](https://aws.amazon.com/blogs/opensource/aws-parallelcluster/) by Mark Duffield
 * [AWS ParallelCluster](https://github.com/aws/aws-parallelcluster) on github
 * [EC2 Instance Pricing](https://aws.amazon.com/ec2/pricing/on-demand/)
 * [EC2 Instance Types](https://aws.amazon.com/ec2/instance-types/)
 * [Connect to the Master Node Using SSH](https://docs.aws.amazon.com/emr/latest/ManagementGuide/emr-connect-master-node-ssh.html)
 * [SGE Manual Pages](http://gridscheduler.sourceforge.net/htmlman/manuals.html)
 * Obsolete [CfnCluster](https://d1.awsstatic.com/Projects/P4114756/deploy-elastic-hpc-cluster_project.pdf) documentation