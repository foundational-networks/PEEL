from os import listdir, scandir, remove
from os.path import isfile, join
import gzip
import shutil

list_subdirs = [f.path for f in scandir('.') if f.is_dir()]

for dir in list_subdirs:
    if './.' not in dir and './_' not in dir and 'RECORDS' not in dir and 'FIGS' not in dir:
        onlyfiles = [f for f in listdir(dir) if isfile(join(dir, f)) and f[-4:] == '.csv']
        print(onlyfiles)
        for i in range(len(onlyfiles)):
            print(dir + '/' + onlyfiles[i])
            with open(dir + '/' + onlyfiles[i], 'rb') as f_in:
                with gzip.open(dir + '/' + onlyfiles[i] + '.gz', 'wb') as f_out:
                    shutil.copyfileobj(f_in, f_out)
            remove(dir + '/' + onlyfiles[i])
