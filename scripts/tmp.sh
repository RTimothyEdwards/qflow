      echo "Running vlog2Verilog." |& tee -a ${synthlog}
      ${bindir}/vlog2Verilog -c -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtl.v ${rootname}_anno.v >>& ${synthlog}

      ${bindir}/vlog2Verilog -c -p -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtlnopwr.v ${rootname}_anno.v >>& ${synthlog}

      ${bindir}/vlog2Verilog -c -b -p -n -v ${vddnet} -g ${gndnet} \
		-o ${rootname}.rtlbb.v ${rootname}_anno.v >>& ${synthlog}

      echo "Running vlog2Spice." |& tee -a ${synthlog}
      ${bindir}/vlog2Spice -i -p ${vddnet} -g ${gndnet} -l ${spicepath} \
		-o ${rootname}.spc ${rootname}_anno.v >>& ${synthlog}

      #------------------------------------------------------------------
      # Spot check:  Did vlog2Verilog or vlog2Spice exit with an error?
      #------------------------------------------------------------------

      if ( !( -f ${rootname}.rtl.v || ( -f ${rootname}.rtl.v && \
		-M ${rootname}.rtl.v < -M ${rootname}_anno.v ))) then
	 echo "vlog2Verilog failure:  No file ${rootname}.rtl.v created." \
		|& tee -a ${synthlog}
      endif

      if ( !( -f ${rootname}.rtlnopwr.v || ( -f ${rootname}.rtlnopwr.v && \
		-M ${rootname}.rtlnopwr.v < -M ${rootname}_anno.v ))) then
	 echo "vlog2Verilog failure:  No file ${rootname}.rtlnopwr.v created." \
		|& tee -a ${synthlog}
      endif

      if ( !( -f ${rootname}.rtlbb.v || ( -f ${rootname}.rtlbb.v && \
		-M ${rootname}.rtlbb.v < -M ${rootname}_anno.v ))) then
	 echo "vlog2Verilog failure:  No file ${rootname}.rtlbb.v created." \
		|& tee -a ${synthlog}
      endif

      if ( !( -f ${rootname}.spc || ( -f ${rootname}.spc && \
		-M ${rootname}.spc < -M ${rootname}_anno.v ))) then
	 echo "vlog2Spice failure:  No file ${rootname}.spc created." \
		|& tee -a ${synthlog}
      endif

