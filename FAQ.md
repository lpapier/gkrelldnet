Single question FAQ
-------------------

    Q: when I try to run 'dnetw' I get the message: "shmget: File exists" ?

That means that `dnetw` can't create his shared memory segment.
First check that there is no other `dnetw` running. You can only run one `dnetw`.
Then check that the file `/tmp/dnetw-shmid` do not exist. You can remove it
only if there is no `dnetw` running.

If the previous conditions are meet and you are using GkrellDnet v0.9 or less,
please upgrade to GkrellDnet v0.10 or better.
