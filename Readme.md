# Multithreaded Job System

## V3 upgrade

This removed all locks and makes the code stable.


## Future versions

Having the ability to add and remove job sources at will would be really nice. This would need to be done with some kind of atomic `updating_job_source_list_status` integer or something.
