void read_touch(){

  // Lee 1 punto de toque “raw”
  int n = M5.Display.getTouchRaw(tp, 1);
  if (n > 0) {

    // Convierte a píxeles
    M5.Display.convertRawXY(tp, n);
      cox=tp[0].x;
      coy=tp[0].y;

      //  Serial.print(cox);
      //  Serial.print(" ");
      //  Serial.println(coy);

    if (!isTouchActive){
      isTouchActive = true; 


      for (byte f=0;f<MAX_BUTTONS;f++){
        if (mButtons[f]->rPage==0 || mButtons[f]->rPage==rPage) {
          if ( (cox > mButtons[f]->x) && (cox < (mButtons[f]->x+mButtons[f]->w)) && (coy > mButtons[f]->y) && (coy < (mButtons[f]->y+mButtons[f]->h)) ) {
            if (f==last_touched ){
              if (start_debounce+debounce_time > millis() ){
                break;
              } 
            }
            mButtons[f]->trigger_on=1;
            last_touched=f;
            start_debounce=millis();
            isTouchActive = true;
            //  Serial.print("b ");
            //  Serial.println(f);
            break;
          }
        }
      }


      for (byte f=0;f<MAX_BARS;f++){
        if (mRotators[f]->rPage==rPage){ // si el controlador pertenece a la pagina seleccionada
          if ( (cox > mRotators[f]->x) && (cox < (mRotators[f]->x+mRotators[f]->w)) && (coy > mRotators[f]->y) && (coy < (mRotators[f]->y+mRotators[f]->h)) ) {
            if (f==last_touched ){
              if (start_debounce+debounce_time > millis() ){
                break;
              } 
            }
            mRotators[f]->trigger_on=1;
            last_touched=f;
            start_debounce=millis();
            isTouchActive = true;
            //Serial.print("r ");
            //Serial.println(f);
            break;
          }
        }
      }

      for (byte f=0;f<16;f++){
        if ( (cox > mSequencers[f]->x) && (cox < (mSequencers[f]->x+mSequencers[f]->w)) && (coy > mSequencers[f]->y) && (coy < (mSequencers[f]->y+mSequencers[f]->h)) ) {
          if (f==last_touched ){
            if (start_debounce+debounce_time > millis() ){
              break;
            } 
          }
          mSequencers[f]->trigger_on=1;
          last_touched=f;
          start_debounce=millis();
          isTouchActive = true;
          //Serial.print("bs ");
          //Serial.println(f);
          break;
        }
      }
    }

  } else {
    isTouchActive = false;
  }

}