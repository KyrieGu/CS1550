public class PTE{

  boolean valid;  //set if this logical page number has a corresponding physical frame in meomry
  int frame;      //page in physical memory
  boolean dirty;  //set if data on the page has been modified
  boolean referenced; //set if data on the page has been accessed
  int virtual;    //virtual address
  String counter;    //page counter
  boolean first;


  //defuat constructor
  public PTE(){
    this.first = false;
    this.valid = false;
    this.frame = -1;
    this.dirty = false;
    this.referenced = false;
    this.virtual = -1;
    this.counter = "00000000";
  }


//intilize everything
  public PTE(boolean valid, boolean dirty, boolean referenced, int frame, int virtual){
    this.first = false;
    this.valid = valid;
    this.frame = frame;
    this.dirty = dirty;
    this.referenced = referenced;
    this.virtual = virtual;
    this.counter = "00000000";
  }

  public PTE(int virtual){
    this.first = false;
    this.virtual = virtual;
    this.valid = false;
    this.frame = -1;
    this.dirty = false;
    this.referenced = false;
    this.counter = "00000000";
  }





}
