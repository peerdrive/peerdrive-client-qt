#ifndef CLI_PEERDRIVE_H
#define CLI_PEERDRIVE_H

class QStringList;

int cmd_mount(const QStringList &args);
int cmd_umount(const QStringList &args);

#endif
