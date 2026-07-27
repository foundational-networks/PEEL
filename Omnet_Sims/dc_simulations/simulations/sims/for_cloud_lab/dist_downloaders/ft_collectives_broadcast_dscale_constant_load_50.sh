sudo mkdir /mnt/data/OneDrive
sudo mkdir /mnt/data/OneDrive/mburst_simulations
sudo mkdir /mnt/data/OneDrive/mburst_simulations/forwarding_dbs
sudo mkdir /mnt/data/OneDrive/mburst_simulations/forwarding_dbs/distributions
sudo git clone https://sepehrabdous96:raGV9hCmJXrQCzHaZ4EX@bitbucket.org/scalable-mcast/ft_collectives_broadcast_dscale_constant_load_50.git /mnt/data/OneDrive/mburst_simulations/forwarding_dbs/distributions/
sudo rm -rf /mnt/data/OneDrive/mburst_simulations/forwarding_dbs/distributions/.git*
sudo bzip2 -d /mnt/data/OneDrive/mburst_simulations/forwarding_dbs/distributions/*.bz2