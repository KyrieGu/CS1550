public class PageTableEntry {
  int index;
  int frame;
  boolean dirty; // each page table entry has to contain these items
  boolean referenced;
  boolean valid;

  public PageTableEntry() {

    this.index = 0;
    this.frame = -1;  //intilize the values
    this.valid = false;
    this.dirty = false;
    this.referenced = false;

  }

  public PageTableEntry(PageTableEntry table1)
  {
    this.index = table1.index;
    this.frame = table1.frame;  //a way to copy page table entries
    this.valid = table1.valid;
    this.dirty = table1.dirty;
    this.referenced = table1.referenced;
  }

}
